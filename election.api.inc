<?php

function _elec_get_format_from_accept_header($accept){
  if(strpos($accept, 'application/json') !==FALSE)
    return 'json';
  elseif(strpos($accept, 'text/html') !==FALSE)
    return 'html';
  elseif(strpos($accept, 'text/csv') !==FALSE)
    return 'csv';
  else{
    header('HTTP/1.0 406 Not Accepted');
    print "The server does not know how to respond to this request\n";
    print_r($_SERVER);
    exit;
  }
}

function _elec_return_as($data, $format, $type){
  switch($format){
    case 'json': 
      print str_replace("\/", "/", json_encode($data));
      exit;
    case 'html':
      return _elec_as_html($data, $type);
      break;
    case 'csv':
      return _elec_as_csv($data);
      break;
    default:
      return _elec_404();
  }
}

function _elec_arg_parse(){
  variable_set('elec_expire_time', 1);
  $cache = _elec_check_cache();
  $accept = $_SERVER['HTTP_ACCEPT'];
  $format = _elec_get_format_from_accept_header($accept);
  $type = 'season';
  if ($cache) {
    $data = unserialize($cache->data);
    _elec_send_modify_expire_headers($cache->expires, $cache->last_update);
    _elec_return_as($data, $format, $type);
    return;
  }
  $args = func_get_args();
  $season = array_shift($args);
  if ( empty($args) ){
    $type = 'season';
    _elec_send_modify_expire_headers();
    return  _elec_return_as(_elec_get_season($season), $format, $type);
  }
  $event_slug = array_shift($args);
  if ( empty($args) ){
    $type = 'event';
    _elec_send_modify_expire_headers();
    return  _elec_return_as(_elec_get_event($season, $event_slug), $format, $type);
  }
  else{
    dpm($args);
  }
  return 'main parse';
}

function _elec_as_html($data,$type){
  switch($type){
    case 'season':
      _elec_html_season($data);
      break;
    case 'event':
      _elec_html_event($data);
      break;
    case 'district':
      _elec_html_district($data);
      break;
  }
  exit;
}

function _elec_html_season($data){
  $data = (object)$data;
  $election_events="<ul>\n";
  foreach($data->election_events as $election_event){
    $election_event = (object)$election_event;
    $election_events .= "\t<li>\n";
    $election_events .= "\t\t<a href=\"" . $election_event->link . "\" class=\"election-event\">$election_event->title</a>\n";
    $election_events .= "\t</li>\n";
  }
  $election_events .= "</ul>";
  print <<<SEASON
<!DOCTYPE html>
<head>
  <title>$data->title</title>
  <link href="$data->link" type="canonical" />
</head>
<body class="election-season">
  <h1 class="title">$data->title</h1>
  <h2>Election Events</h2>
  $election_events
</body>
SEASON;
  exit;
}

function _elec_html_district($data){
  print_r($data);
  exit;
}

function _elec_html_event($data){
  $data = (object)$data;
  $election_season_link = '<a href="' . $data->election_season['link'] . '">'. $data->election_season['title'] . '</a>';
  $district_type_containers = array_slice((array)$data, 3, NULL, true); //remove title, link, election-season
  $election_events = '';
  foreach($district_type_containers as $key => $container){
    $election_events .= '<h2>' . ucfirst($key) . '</h2>' . "\n"; 
    $election_events .= '    <ul>' . "\n";
    foreach($container as $e_event){
      $election_events .= '      <li><a href="' . $e_event['link'] . '" class="district">' . $e_event['title'] . '</a></li>' . "\n";
    }
    $election_events .= "    </ul>";
  }
  print <<<EVENT
<!DOCTYPE html>
<head>
  <title>$data->title</title>
  <link href="$data->link" type="canonical" />
</head>
<body class="election-event">
  <h1 class="title">$data->title</h1>
  <dl>
    <dt>Election Season</dt>
    <dd>$election_season_link</dd>
  </dl>
  <h2>Election Events</h2>
  $election_events
</body>
EVENT;
  exit;
}

function _elec_as_csv($data){
  print_r($data);
  exit;
}

function _elec_get_event($season_slug, $event_slug){
  $season_node = node_load(array('title'=> _elec_slug_to_title($season_slug), 'type'=>'elec_election_season'), NULL, TRUE); 
  if (!$season_node){
    _elec_404("Season not found");
  }
  $event_nid = db_result(db_query("SELECT ee.field_elec_election_events_nid FROM node n JOIN content_field_elec_election_events ee ON n.nid = ee.field_elec_election_events_nid WHERE n.title = \"%s\" AND ee.nid=%d", _elec_slug_to_title($event_slug), $season_node->nid));
  if (!$event_nid){
    _elec_404("Event $event_slug not found for season $season_slug");
  }
  $event_node = node_load(array('nid'=>$event_nid));
  $event_link = $_GET['q'];
  $data = array(
    'title' => $event_node->title,
    'link' => $event_link,
    'election_season' => array(
      'title' => $season_node->title,
      'link' => 'e/' . $season_slug,
    ),
  );  
  $districts_data = array();
  foreach($event_node->field_elec_participating_distric as $district){
    $district_node = node_load(array('nid' => $district['nid']));
    $district_taxo_vid = variable_get('elec_district_vid', NULL);
    $district_types = array();
    foreach($district_node->taxonomy as $term){
      if($term->vid == $district_taxo_vid)
        $district_types[] = $term->name;
    }
    foreach($district_types as $dt){
      $districts_data[$dt][] = array(
        'title' => $district_node->title,    
        'link' => $event_link . '/' . _elec_title_to_slug($district_node->title),
      );
    }
  }
  $districts_data = _elec_fix_district_keys($districts_data);
  $data = array_merge($data, $districts_data);
  _elec_write_cache($_GET['q'], $data);
  return $data;
}

function _elec_fix_district_keys($district_data){
  if( isset($district_data['Municipality']) ){
    $district_data['municipalities'] = $district_data['Municipality'];
    unset($district_data['Municipality']);
  }
  if( isset($district_data['District']) ){
    $district_data['districts'] = $district_data['District'];
    unset($district_data['District']);
  }
  return $district_data;
}

function _elec_get_season($season_slug){
  $season_node = node_load(array('title'=> _elec_slug_to_title($season_slug), 'type'=>'elec_election_season'), NULL, TRUE); 
  if (!$season_node){
    _elec_404("Season not found");
  }

  $season_link = $_GET['q'];

  $election_events = array();
  foreach($season_node->field_elec_election_events as $key => $election_event){
    $e_node = node_load(array('nid'=>$election_event['nid']));
    $e_event = array(
      'title' => $e_node->title,
      'link'  => $season_link . '/' . _elec_title_to_slug($e_node->title),
    );
    $election_events[] = $e_event;
  }
  $data = array(
    'title' => $season_node->title, 
    'link' => $season_link,
    'election_events' => $election_events,
  );
  _elec_write_cache($_GET['q'], $data);
  return $data;
}


function _elec_404($message){
  header('HTTP/1.0 404 Not Found');
  print $message . "\n";
  exit;
}

function _elec_slug_to_title($slug){
  return str_replace('-',' ', $slug);
}


function _elec_title_to_slug($title){
  return strtolower(str_replace(' ', '-', $title));
}


function _elec_write_cache($uri, $data){
  $query = 'INSERT INTO {election_cache} (uri,last_update,expires,data) VALUES ("%s", %d, %d, "%s") ON DUPLICATE KEY UPDATE last_update=%d, expires=%d, data="%s"';
  $last_update = time();
  $expires = $last_update + variable_get('elec_expire_time', 60);
  db_query($query, $uri, $last_update, $expires, serialize($data), $last_update, $expires, serialize($data));
}

function _elec_check_cache($uri = NULL){
  if( ! $uri )
    $uri = $_GET['q'];
  return db_fetch_object(db_query('SELECT data, last_update,expires from {election_cache} where uri="%s" AND expires > %d', $uri, time()));
}

function _elec_send_modify_expire_headers($expires = null, $last_modified = null){
  $last_modified = ($last_modified) ? $last_modified : time();
  $expires = ($expires) ? $expires : time() + variable_get('elec_expire_time', 60);
  header('Expires: ' . date("D, d M Y H:i:s e", $expires));
  header('Last-Modified: ' . date("D, d M Y H:i:s e", $last_modified));
}
