<?php

$content['type']  = array (
  'name' => 'Election Season',
  'type' => 'elec_election_season',
  'description' => '',
  'title_label' => 'Title',
  'body_label' => 'Body',
  'min_word_count' => '0',
  'help' => '',
  'node_options' => 
  array (
    'status' => true,
    'promote' => true,
    'sticky' => false,
    'revision' => false,
  ),
  'old_type' => 'elec_season',
  'orig_type' => '',
  'module' => 'node',
  'custom' => '1',
  'modified' => '1',
  'locked' => '0',
);
$content['fields']  = array (
  0 => 
  array (
    'label' => 'Election Events',
    'field_name' => 'field_elec_election_events',
    'type' => 'nodereference',
    'widget_type' => 'nodereference_autocomplete',
    'change' => 'Change basic information',
    'weight' => '-3',
    'autocomplete_match' => 'contains',
    'size' => '60',
    'description' => '',
    'default_value' => 
    array (
      0 => 
      array (
        'nid' => NULL,
        '_error_element' => 'default_value_widget][field_elec_election_events][0][nid][nid',
      ),
    ),
    'default_value_php' => '',
    'default_value_widget' => NULL,
    'group' => false,
    'required' => 0,
    'multiple' => '1',
    'referenceable_types' => 
    array (
      'elec_election_event' => 'elec_election_event',
      'elec_ballot_item' => 0,
      'elec_district' => 0,
      'elec_election_season' => 0,
    ),
    'op' => 'Save field settings',
    'module' => 'nodereference',
    'widget_module' => 'nodereference',
    'columns' => 
    array (
      'nid' => 
      array (
        'type' => 'int',
        'unsigned' => true,
        'not null' => false,
        'index' => true,
      ),
    ),
    'display_settings' => 
    array (
      'label' => 
      array (
        'format' => 'above',
        'exclude' => 0,
      ),
      'teaser' => 
      array (
        'format' => 'default',
        'exclude' => 0,
      ),
      'full' => 
      array (
        'format' => 'default',
        'exclude' => 0,
      ),
      4 => 
      array (
        'format' => 'default',
        'exclude' => 0,
      ),
    ),
  ),
);
$content['extra']  = array (
  'title' => '-5',
  'body_field' => '-4',
  'revision_information' => '0',
  'author' => '-1',
  'options' => '1',
  'menu' => '-2',
);

