<?php

function book_management_wrapper($pid) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  return drupal_render(get_page_model_management_content($pid));
}

function get_book_model_management_content($pid) {
  $form['manage_book_tab'] = array(
    '#type' => 'tabpage',
    '#title' => t('Manage This Book'),
    '#content' => drupal_get_form('book_management_form', $pid),
    '#selected' => FALSE,
  );
  return $form;
}

function book_management_form(&$form_state, $pid) {

  module_load_include('inc', 'islandora_book', 'book_pack_utils');
  module_load_include('inc', 'fedora_repository', 'plugins/FedoraObjectDetailedContent');
  $content_helper = new FedoraObjectDetailedContent($pid);
  $objectHelper = new ObjectHelper();
  $ds_list = datastream_display_builder($content_helper->pid, $content_helper->item);
  $add_datastreams_details = get_add_datastream_data($content_helper->pid);
  $dc_html = $objectHelper->getFormattedDC($content_helper->item);
  $purge_form = drupal_get_form('fedora_repository_purge_object_form', $content_helper->pid, check_plain(substr(request_uri(), strlen(base_path()))));
  $potential_collections = get_collections_as_option_array();
  $current_collections = get_collection_relationships($pid);

  $values = array_values($current_collections);
  $collection_value = $values[0];
  $keys = array_keys($current_collections);
  $collection_key = $keys[0];

  unset($potential_collections[$collection_key]);
  $values = array_values($current_collections);
  $form = array();
  $form['pid'] = array(
    '#type' => 'hidden',
    '#value' => $pid,
  );
  $form['current_collection'] = array(
    '#type' => 'hidden',
    '#value' => $collection_key,
  );
  $form['collections'] = array(
    '#type' => 'fieldset',
    '#collapsed' => true,
    '#collapsible' => true,
    '#title' => t('Collection Membership'),
  );


  $form['collections']['current'] = array(
    '#title' => 'Currently Member of',
    '#type' => 'item',
    '#value' => $collection_value,
  );

  $form['collections']['new_collection'] = array(
    '#title' => 'New Collection',
    '#description' => t("Move book object to new collection"),
    '#type' => 'select',
    '#options' => $potential_collections,
  );

  $form['collections']['submit'] = array(
    '#type' => 'submit',
    '#id' => 'update_collection',
    '#value' => t('Move Book to New Collection'),
  );


  $form['view_dc'] = array(
    '#type' => 'fieldset',
    '#collapsed' => true,
    '#collapsible' => true,
    '#title' => t('View Metadata'),
  );

  $form['view_dc']['dublin_core'] = array(
    '#type' => 'tabpage',
    '#title' => t('View'),
    '#content' => $dc_html,
  );
  $form['page_manage'] = array(
    '#type' => 'fieldset',
    '#collapsed' => true,
    '#collapsible' => true,
    '#title' => t('Update Derived Datastreams'),
    '#description' => t('Update datastreams for all pages in this book.  The stored tiffs will be used to create all derived datastreams.'),
  );
  $form['page_manage']['do_ocr'] = array(
    '#type' => 'checkbox',
    '#title' => t('Refresh OCR for this book')
  );
  $form['page_manage']['recreate_derivatives'] = array(
    '#type' => 'checkbox',
    '#title' => t('Refresh all derived images')
  );

  $form['page_manage']['submit'] = array(
    '#type' => 'submit',
    '#id' => 'refresh_datastreams',
    '#value' => t('Update Page Datastreams'),
  );
  $form['current_datastreams'] = array(
    '#type' => 'fieldset',
    '#collapsed' => true,
    '#collapsible' => true,
    '#title' => t('Manage Current Datastreams'),
  );

  $form['current_datastreams']['current_object_details'] = array(
    '#type' => 'tabpage',
    '#title' => t('View'),
    '#content' => $ds_list,
  );
  if (count($add_datastreams_details) > 0) {
    $form['current_datastreams']['datastream_candidates'] = array(
      '#type' => 'tabpage',
      '#title' => t('View'),
      '#content' => $add_datastreams_details,
    );
  }

  if (user_access('delete pages')) {
    $form['delete_page'] = array(
      '#title' => "Permanently Delete this Book",
      '#type' => 'fieldset',
      '#description' => t("Clicking this button will delete this book. <br /> <strong>This action cannot be undone.</strong>"),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    );
    $form['delete_page']['confirm'] = array(
      '#title' => "Are you sure?",
      '#type' => 'fieldset',
      '#description' => t('<strong>Clicking the delete button will permanantly remove this book and all of its pages. <br />  <strong>This action cannot be undone.</strong> '),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    );

    $form['delete_page']['confirm']['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Delete this book'),
      '#id' => 'delete_page',
    );
  }

  return $form;
}

function book_management_form_submit($form, &$form_state) {
  global $user;
  $book_pid = $form_state['values']['pid'];
  $collection_pids = get_collection_from_pid($book_pid);
  $item = new Fedora_Item($book_pid);
  if ($form_state['clicked_button']['#id'] == 'update_collection') {
    $current_collection = $form_state['values']['current_collection'];
    $item->purge_relationship('isMemberOfCollection', $current_collection);
    $new_collection = $form_state['values']['new_collection'];
    $item->add_relationship('isMemberOfCollection', $new_collection, RELS_EXT_URI);
    drupal_goto("fedora/repository/$new_collection");
  }

  if ($form_state['clicked_button']['#id'] == 'refresh_datastreams') {
    if ($form_state['values']['do_ocr']) {
      $do_ocr = TRUE;
    }
    if ($form_state['values']['recreate_derivatives']) {
      $refresh_images = TRUE;
    }
    book_batch_update_pages($book_pid, $do_ocr, $refresh_images);
  }
  if ($form_state['clicked_button']['#id'] == 'delete_page') {
    $page_pids = get_page_pids($book_pid);
    foreach ($page_pids as $page_pid) {
      $item = new Fedora_Item($page_pid);
      $item->purge($pid . ' deleted by user identified as ' . $user->name);
    }
    $item = new Fedora_Item($book_pid);
    $item->purge($book_pid . ' deleted by user identified as ' . $user->name);
  }
  drupal_goto('fedora/repository/' . $collection_pids[0]);
}

function book_batch_update_pages($book_pid, $do_ocr, $refresh_images) {
  $page_pids = get_page_pids($book_pid);
  $count = count($page_pids);
  $batch = array(
    'title' => "updating $count page objects",
    'operations' => array(),
    'file' => drupal_get_path('module', 'islandora_book') . '/book_object_manager.inc',
  );

  foreach ($page_pids as $page_pid) {
    $batch['operations'][] = array('update_individual_pages', array($page_pid, $do_ocr, $refresh_images));
  }
  if ($refresh_images) {
    $batch['operations'][] = array('replace_cover_thumbnail', array($book_pid, reset($page_pids)));
  }
  batch_set($batch);
  batch_process("fedora/repository/$book_pid");
}

function update_individual_pages($page_pid, $do_ocr, $refresh_images) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  module_load_include('inc', 'islandora_book', 'book_pack_utils');
  $item = new Fedora_Item($page_pid);
  $tiff_file_path = save_datastream_as_tempfile($page_pid, 'TIFF');
  if ($do_ocr) {
    refresh_ocr($tiff_file_path, $item);
  }
  if ($refresh_images) {
    refresh_images($tiff_file_path, $item);
  }
  file_delete($tiff_file_path);
}

function replace_cover_thumbnail($book_pid, $page_pid) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  module_load_include('inc', 'islandora_book', 'book_pack_utils');
  $file = save_datastream_as_tempfile($page_pid, 'TN');
  $item = new fedora_item($book_pid);
  $datastreams = $item->get_datastreams_list_as_array();
  if (array_key_exists('TN', $datastreams)) {
    $item->purge_datastream('TN');
  }
  $item->add_datastream_from_file("$file", 'TN', 'Thumbnail', 'image/jpeg', 'M');
  file_delete($file);
}