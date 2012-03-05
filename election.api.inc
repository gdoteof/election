<?php

function _elec_get_format_from_accept_header(){
  $accept = $_SERVER['HTTP_ACCEPT'];
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
      header('Content-type: application/json');
      print str_replace("\/", "/", json_encode($data));
      exit;
    case 'html':
      return _elec_as_html($data, $type);
      break;
    case 'csv':
      return _elec_as_csv($data, $type);
      break;
    default:
      return _elec_404();
  }
}

function _elec_arg_parse(){
  $format = _elec_get_format_from_accept_header();
  if(substr($_GET['q'], -4) == '.csv'){
    $format = "csv";
    $uri = substr($_GET['q'], 0, -4);
  }elseif(substr($_GET['q'], -4) == '.skl'){
    $format = "html";
    $uri = substr($_GET['q'], 0, -4);
  }
  else{
    $uri = $_GET['q'];
  }
  $cache = _elec_check_cache($uri);
  $type = 'season';
  switch(count(func_get_args())){
    case 1: $type = 'season'; break;
    case 2: $type = 'event'; break;
    case 3: $type = 'district'; break;
    case 4: $type = 'ballot_item'; break;
  }
  if ($cache) {
    $data = unserialize($cache->data);
    if( isset($_SERVER['HTTP_IF_MODIFIED_SINCE']) ){
      $if_since = strtotime($_SERVER['HTTP_IF_MODIFIED_SINCE']);
      if($if_since > $cache->last_update){ //The content has not been modified since the specified time
        _elec_304(); 
        return; //never called, just for readability.
      }
    }
    _elec_send_modify_expire_headers($cache->expires, $cache->last_update);
    _elec_return_as($data, $format, $type);
    return;
  }
  $args = func_get_args();
  $season = array_shift($args);
  if ( empty($args) ){
    $type = 'season';
    $season = _elec_remove_csv_extension($season);
    _elec_send_modify_expire_headers();
    return  _elec_return_as(_elec_get_season($season), $format, $type);
  }
  $event_slug = array_shift($args);
  if ( empty($args) ){
    $event_slug = _elec_remove_csv_extension($event_slug);
    $type = 'event';
    _elec_send_modify_expire_headers();
    return  _elec_return_as(_elec_get_event($season, $event_slug), $format, $type);
  }
  $district_slug = array_shift($args);
  if ( empty($args) ){
    $district_slug = _elec_remove_csv_extension($district_slug);
    $type = 'district';
    _elec_send_modify_expire_headers();
    return  _elec_return_as(_elec_get_district($season, $event_slug, $district_slug), $format, $type);
  }
  $ballot_item_slug = array_shift($args);
  if (empty($args)){
    $ballot_item_slug = _elec_remove_csv_extension($ballot_item_slug);
    $type = 'ballot_item';
    _elec_send_modify_expire_headers();
    return _elec_return_as(_elec_get_ballot_item($season, $event_slug, $district_slug, $ballot_item_slug), $format, $type);
  }
  else{
//    dpm($args);
  }
  return 'main parse';
}

function _elec_as_html($data,$type){
  switch($type){
    case 'season':
      _elec_html_season($data);
      break;
    case 'event':
      /* a bit clunky, but should give us the skeleton refresh method as part of the API*/  
      if(substr($_GET['q'], -4) == '.skl'){
        _elec_html_event_skeleton($data);
      }else{
        _elec_html_event($data);
      }
      break;
    case 'district':
      _elec_html_district($data);
      break;
    case 'ballot_item':
      _elec_html_ballot_item($data);
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
    $election_events .= "\t\t<a href=\"/" . $election_event->link . "\" class=\"election-event\">$election_event->title</a>\n";
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

function _elec_html_ballot_item($data){
  $data = (object)$data;
  $election_season_href = $data->election_season['link'];
  $election_season_title = $data->election_season['title'];
  $election_event_href = $data->election_event['link'];
  $election_event_title = $data->election_event['title'];
  $district_href = $data->district['link'];
  $district_title = $data->district['title'];

  $tags = "<ul>\n";
  foreach ($data->tags as $tag){ $type = strtolower($tag); $tags .= '    <li><abbr title="'.$tag.'" class="tag">'.$tag.'</abbr></li>' ."\n"; }
  $tags .="  </ul>\n";

  $total_votes = 0;
  foreach ($data->results as $result){ $total_votes += $result['votes']; $complete = $result['complete']; }

  $results = <<<TABLE
  <table>
    <thead>
      <tr class="result">
        <th class="option candidate">Candidate</th>
        <th class="votes">Votes</th>
TABLE;
  if($complete) $results .= '<th class="percent">Percent</th>';
  $results .= <<<TABLE
      </tr>
    </thead>
    <tbody>
TABLE;
  foreach ($data->results as $option => $result){
    $winner = ($result['winner']) ? 'winner' : '';
    $complete = ($result['complete']) ? ' complete ' : '';
    $percent = ($complete) ? number_format($result['votes'] / $total_votes * 100, 1) : 'n/a';
    $opt_type = ($type=='election') ? 'candidate' : 'boolean';
    $results .= '  <tr class="result ' . $winner . $complete . '">' . "\n";
    $results .= '    <td class="option ' . $opt_type . '">' . $option . "</td>\n";
    $results .= '    <td class="votes"> '. $result['votes'] . "</td>\n";
    if($result['complete']){ $results .= '    <td class="percent">' .  $percent . "</td>\n"; }
    $results .= '  </tr>'. "\n";
  }
  $results .= "</tbody></table>";
  print <<<BALLOT
<!DOCTYPE html>
<head>
  <title>$data->title</title>
  <link href="$data->link" type="canonical" />
</head>
<body class="ballot-item $type">
  <h1 class="title">$data->title</h1>
  <dl>
    <dt>Election Season</dt>
    <dd><a href="$election_season_href" class="election-season">$election_season_title</a></dd>
    <dt>Election Event</dt>
    <dd><a href="$election_event_href" class="election-event">$election_event_title</a></dd>
    <dt>District</dt>
    <dd><a href="$district_href" class="election-event">$district_title</a></dd>
  </dl>
  <h2>Tags</h2>
  $tags
  <h2>Results</h2>
  $results
</body>
BALLOT;
}

function _elec_html_ballot_item_footer($data){
  $first_item = array_pop(array_slice($data->results, 0, 1));
  if( !isset($first_item['winner']) ){
    return ' <p class="partial">These are partial results.</p>';
  }
  elseif( $first_item['complete'] ){
    return ' <p class="complete">These are complete but unofficial results</p>';
  }
  else{
    return ' <p class="complete">These are complete but unofficial results</p>';
  }
}

function _elec_html_ballot_item_skel($data){
  $data = (object)$data;
  $election_season_href = $data->election_season['link'];
  $election_season_title = $data->election_season['title'];
  //$election_season_title = $data->election_season['title'];
  $season_slug = _elec_title_to_slug($election_season_title);
  $election_event_href = $data->election_event['link'];
  $election_event_title = $data->election_event['title'];
  $event_slug = _elec_title_to_slug($election_event_title);
  $district_href = $data->district['link'];
  $district_title = $data->district['title'];
  $district_slug = _elec_title_to_slug($district_title);
  $ballot_item_title = ($data->text) ? ($data->text) : $data->title;
  $last_update = strftime('%b %e, %Y %r',mktime());
  $ballot_item_slug = _elec_title_to_slug($ballot_item_title);
  $data_link = base_path() . 'e/' . $season_slug .'/'. $event_slug . '/' . $district_slug . '/' . $ballot_item_slug;

  $tags = "<ul>\n";
  foreach ($data->tags as $tag){ $type = strtolower($tag); $tags .= '    <li><abbr title="'.$tag.'" class="tag">'.$tag.'</abbr></li>' ."\n"; }
  $tags .="  </ul>\n";

  $total_votes = 0;
  $results = '<article id="' . $district_slug . '-' . $ballot_item_slug . '" class="ballot-item-results ' . strtolower($tag) . '" data-link="' .$data_link . '">';
  $results .='<h1 id="' . $district_slug . '-' . $ballot_item_slug . '-h1' . '">' . $ballot_item_title . '</h1>';
  foreach ($data->results as $result){ $total_votes += $result['votes']; $complete = $result['complete']; }
  $opt_type_th = ($type=='election') ? 'Candidates' : 'Options';
  $opt_type_class = ($type=='election') ? 'candidate' : '';
  $results .= <<<TABLE
  <table>
    <thead>
      <tr class="result">
        <th class="option $opt_type_class"><span class="graph-swatch"></span>$opt_type_th</th>
        <th class="votes">Votes</th>
TABLE;
  if($complete) $results .= '<th class="percent">Percent</th>';
  $results .= <<<TABLE
      </tr>
    </thead>
    <tbody>
TABLE;
  $graph = '<ol class="graph">' . "\n";
  $footer = '<footer>' . "\n";
  foreach ($data->results as $option => $result){
    $winner = ($result['winner']) ? 'winner' : '';
    $complete = ($result['complete']) ? ' complete ' : '';
    $percent = ($complete) ? number_format($result['votes'] / $total_votes * 100, 1) : 'n/a';
    $opt_type = ($type=='election') ? 'candidate' : '';
    $results .= '  <tr class="result ' . $winner . $complete . '">' . "\n";
    $results .= '    <td class="option ' . $opt_type . '"><span class="graph-swatch"></span>' . $option . "</td>\n";
    $results .= '    <td class="votes"> '. $result['votes'] . "</td>\n";
    $footer .= '' . "\n";
    $graph .= '<li class="result ' . $opt_type  . '" title="' . $option . ' : ' . $result['votes'] . ' votes, ' . $percent . "%".'" style="width: ' . $percent . "%" . '"><span class="option">' . $option . '</span> : <span class="votes">' . $result['votes']  . '</span> votes, <span class="percent">' . $percent . '</span>%</li>' . "\n";
    if($result['complete']){ $results .= '    <td class="percent">' .  $percent . "</td>\n"; }
    $results .= '  </tr>'. "\n";
  }
  $results .= "</tbody></table>" . "\n";
  if ($complete){
    $results .=  $graph . '</ol>' . "\n";
  }else{
    $results .= '<ol class="graph"></ol>' . "\n";
  }
  $results .= $footer . _elec_html_ballot_item_footer($data) . "\n" . '<p>Updated <span class="updated">' . $last_update . '</span></p>' . "\n" . '</footer>';
  //$results .= $footer . 'generated p first row footer here' . "\n" . '<p>Updated <span class="updated">' . $last_update . '</span></p>' . "\n" . '</footer>';
  $results .= '</article>' . "\n";
  //print $results;
return $results;
}

function _elec_html_district($data){
  $data = (object)$data;
  $election_season_href = $data->election_season['link'];
  $election_season_title = $data->election_season['title'];
  $election_event_href = $data->election_event['link'];
  $election_event_title = $data->election_event['title'];

  $tags = "<ul>\n";
  foreach ($data->tags as $tag){ $tags .= '    <li><abbr title="'.$tag.'" class="tag">'.$tag.'</abbr></li>' ."\n"; }
  $tags .="  </ul>\n";

  $ballot_items = '';
  foreach ($data->ballot_items as $ballot_item_type => $ballot_item_list){
  $ballot_items .= "    <h3>" . ucfirst($ballot_item_type) .  "</h3>\n<ul>\n";
    foreach($ballot_item_list as $ballot_item){
      $ballot_items .= '      <li><a href="' . $ballot_item['link'] .  '" class="ballot-item ' . _elec_title_to_slug(_elec_swap_plurality($ballot_item_type)) . '">' . $ballot_item['title'] . '</a></li>' . "\n"; 
    }
    $ballot_items .= "    </ul>\n";
  }

  $districts = '';
  foreach ($data->districts as $district_type => $district_list){
  $district_type = ucwords(str_replace('-', ' ',$district_type));
  $districts .= "<h2>" . $district_type . "</h2>\n<ul>\n";
    foreach ($district_list as $district){
      $districts .= '      <li><a href="' . $district['link'] .  '" class="district ' . str_replace(' ','-',_elec_swap_plurality($district_type)) . '">' . $district['title'] . '</a></li>' . "\n"; 
    }
    $districts .= "</ul>\n";
  }
  print <<<DISTRICT
<!DOCTYPE html>
<head>
  <title>$data->title</title>
  <link href="$data->link" type="canonical" />
</head>
<body class="election-season">
  <h1 class="title">$data->title</h1>
  <dl>
    <dt>Election Season</dt>
    <dd><a href="$election_season_href" class="election-season">$election_season_title</a></dd>
    <dt>Election Event</dt>
    <dd><a href="$election_event_href" class="election-event">$election_event_title</a></dd>
  </dl>
  <h2>Tags</h2>
  $tags
  <h2>Ballot Items</h2>
$ballot_items
  <h2>Districts</h2>
$districts
</body>
DISTRICT;
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
      $election_events .= '      <li><a href="/' . $e_event['link'] . '" class="district '. _elec_swap_plurality($key) . '">' . $e_event['title'] . '</a></li>' . "\n";
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
  $election_events
</body>
EVENT;
  exit;
}

/*builds the HTML skeleton and nav interface for the results page -
we need to be able to call this function with the election event $data.
This is a very heavy function and so the final call dumps it to a
uniquely named .php file baed on the election event nid. Inside it is
just the string variable storing all the results and a print command.
It is then included as a php file (via nid to reconstruct filename) from the file cache
into the theme, where it evaluates and prints.  -AC*/

function _elec_html_event_skeleton($data){
  //print "<pre>"; print_r($data); print "</pre>";
  $data = (object)$data;

  $season_slug = _elec_last_uri_path($data->election_season['link']);
  $season_nid = _elec_nid_from_season_slug($season_slug);
  $event_slug = _elec_last_uri_path(substr($_GET['q'], 0, -4));
  //$event_slug = _elec_last_uri_path($_SERVER["REQUEST_URI"]);
  $event_nid = _elec_nid_from_event_slug($event_slug, $season_nid);

  $district_type_containers = array_slice((array)$data, 3, NULL, true); //remove title, link, election-season- GG

  /*$election_nav = '<p>'. $season_slug . '<br/>' . $event_slug . '</p>' . '<pre>' . print_r($data) . '</pre>' . "\n";*/
  $election_nav .= '<nav>' . "\n"; //collect html for nav
  $ballot_items = '';   //collect html for results
  foreach($district_type_containers as $key => $container){
    //only use municipalities.
    if($key == "municipalities"){   
    foreach($container as $e_event){
      $district_slug = _elec_title_to_slug($e_event['title']);
      $district_nid = _elec_nid_from_district_slug($district_slug, $event_nid);
      $election_nav .= '<a href="#' . $e_event['title'] . '">' . $e_event['title'] . '</a>' . "\n";/* district nav */
      $ballot_items .= '<article id="' . $district_slug . '" class="district-results">' . "\n" . '<h1 id="' . $e_event['title'] . '">' . ucfirst(_elec_slug_to_title($district_slug)) . '</h1>' . "\n" . '</article>' . "\n";/* district header */
      //$district_data = _elec_get_ballot_item_results_by_district($district_nid);
      $district_data = _elec_get_district($season_slug, $event_slug, $district_slug);
      /*$district_data = _  need burlington sub districts branch? summed */
      
      $district_data = (object)$district_data;

//start of ugliness
      if(! empty($district_data->districts)){
        //SCARY BADNESS!!!!!!!!!!!!!  This thing maniuplates $ballot_items
        _elec_make_summed_district_data($district_nid, $ballot_items);
      }
//end of ugliness
      /*

      $ballot_items .= ''.print_r($district_data).' ';
      print '<pre>'; 
      print_r($container);
      print_r($district_data->districts); 
      print '</pre>';
      */
      _elec_skel_eat_subdistricts($district_data->districts, $event_nid,  $season_slug, $event_slug, $ballot_items, $election_nav);
      foreach ($district_data->ballot_items as $ballot_item_type => $ballot_item_list){
        /*$election_nav .= '<a href="#' . $e_event['title'] . ' ' . ucfirst($ballot_item_type) . '">' . ucfirst($ballot_item_type) . '</a>' . "\n"; ballot-item tag nav */
        $ballot_items .= "\n" . '<h1 id="' . $e_event['title'] . ' ' . ucfirst($ballot_item_type) . '">' . ucfirst($ballot_item_type) . '</h1>' . "\n";/* district ballot-item type anchor and title */

        foreach($ballot_item_list as $ballot_item){

          $ballot_item_slug = _elec_title_to_slug($ballot_item['title']);
          $ballot_items_data = _elec_get_ballot_item($season_slug,$event_slug,$district_slug,$ballot_item_slug);
          //print "ballot item data:<pre>"; print_r($ballot_items_data); print "</pre>";
          $ballot_items .= _elec_html_ballot_item_skel($ballot_items_data);
        }/*ballot item loop*/

      }/*ballot item type loop*/

    }/*district loop*/

    }/* endif to do just municipalities*/

  }/*district type loop*/
  $ballot_items .= "\n";/*end results html*/
  $election_nav .= "</nav>" . "\n";/*end nav html*/
  $elec_results = '<div id ="nav_wrapper" class="election-event-nav">' . "\n" . $election_nav . "\n" . '</div>' . "\n" . '<div id="results_wrapper"  class="election-event-results">' . "\n" . $ballot_items . "\n" . '</div>';
  print $elec_results;
  _elec_html_skel_to_file($event_nid,$elec_results);
  exit;
};

function _elec_skel_eat_subdistricts($sub_districts, $event_nid, $season_slug, $event_slug,  &$ballot_items, &$election_nav, $depth = 1){
  if(empty($sub_districts) ) return;
  //print "(depth=$depth)sub distrcts:<pre>"; print_r($sub_districts); print "</pre>";
  foreach($sub_districts as $key => $container){

    foreach($container as $e_event){
      $district_slug = _elec_title_to_slug($e_event['title']);
      $district_nid = _elec_nid_from_district_slug($district_slug, $event_nid);
      $election_nav .= '<a href="#' . $e_event['title'] . '">' . $e_event['title'] . '</a>' . "\n";/* district nav */
      $ballot_items .= '<article id="' . $district_slug . '" class="district-results">' . "\n" . '<h1 id="' . $e_event['title'] . '">' . ucfirst(_elec_slug_to_title($district_slug)) . '</h1>' . "\n" . '</article>' . "\n";/* district header */
      //$district_data = _elec_get_ballot_item_results_by_district($district_nid);
      $district_data = _elec_get_district($season_slug, $event_slug, $district_slug);
      $summed_ballot_items = _elec_get_ballot_item_results_by_district($district_nid);
      //print "calling with district nid $district_nid ($district_slug) </br>";
      //print "summed ballot items:<pre>"; print_r($summed_ballot_items); print "</pre>";
      /*$district_data = _  need burlington sub districts branch? summed */
      $district_data = (object)$district_data;
//start of ugliness
      if(! empty($district_data->districts)){
        //SCARY BADNESS!!!!!!!!!!!!!  This thing maniuplates $ballot_items
        _elec_make_summed_district_data($district_nid, $ballot_items);
      }
//end of ugliness
      /*
      $ballot_items .= ''.print_r($district_data).' ';
      print "key is $key; container is ";
      print '<pre>'; 
      print_r($container);
      print_r($district_data->districts); 
      print '</pre>';
      */
      _elec_skel_eat_subdistricts($district_data->districts, $event_nid, $season_slug, $event_slug, $ballot_items, $election_nav, $depth+1);
      foreach ($district_data->ballot_items as $ballot_item_type => $ballot_item_list){
        /*$election_nav .= '<a href="#' . $e_event['title'] . ' ' . ucfirst($ballot_item_type) . '">' . ucfirst($ballot_item_type) . '</a>' . "\n"; ballot-item tag nav */
        $ballot_items .= "\n" . '<h1 id="' . $e_event['title'] . ' ' . ucfirst($ballot_item_type) . '">' . ucfirst($ballot_item_type) . '</h1>' . "\n";/* district ballot-item type anchor and title */

        foreach($ballot_item_list as $ballot_item){

          $ballot_item_slug = _elec_title_to_slug($ballot_item['title']);
          $ballot_items_data = _elec_get_ballot_item($season_slug,$event_slug,$district_slug,$ballot_item_slug);
          $ballot_items .= _elec_html_ballot_item_skel($ballot_items_data);

        }/*ballot item loop*/

      }/*ballot item type loop*/

    }/*district loop*/
  }
}

/* result HTML text to a php file, clobbers old file, likely a big potential security risk writing a php file in files/election */
function _elec_html_skel_to_file($event_nid,$elec_results){
  $file = file_directory_path() . '/' . 'election' . '/' . strval($event_nid) . '-elec-skeleton' . '.php';
  //$elec_results =  "here is some cached HTML skeleton from the election module";
  $output = '<?php $results = \'' . $elec_results . '\'; ' . "\n" . 'print $results; ?>';
  //print($file);
  $filehandle = fopen( $file , "w");
  fwrite($filehandle, $output);
  fclose($filehandle);
};



function _elec_as_csv($data,$type){
  $filename = str_replace('/','_', $_GET['q']);
  if($type != 'ballot_item' && $type != 'district'){
    _elec_406();
  }
  if($type == 'district'){
    $output = array();
    foreach($data['ballot_item_results'] as $key => $ballot_item){
      $output[] = array($ballot_item['text']);
      unset($ballot_item['text']);
      // bug with "Array" string formatting - remember to fix AC - 
      $options = array(''); $results = array('');
      foreach($ballot_item['results'] as $option => $result){
        $options[] = str_replace('_',' ', $option); 
        $results[] = $result['votes'];
      }
      $output[] = $options;
      $output[] = $results;
    }
  }
  header('Content-Disposition: attachment; filename="'. $filename  . '"');
  $outstream = fopen("php://output",'w'); 
  foreach($output as $row){
    fputcsv($outstream, $row, ',', '"');
  }
  fclose($outstream);
  exit;
}

function _elec_get_event($season_slug, $event_slug){
  $season_node = node_load(array('title'=> _elec_slug_to_title($season_slug), 'type'=>'elec_election_season'), NULL, TRUE); 
  $season_nid = _elec_nid_from_season_slug($season_slug);
  if (!$season_nid){
    _elec_404("Season not found");
  }
  $event_nid = _elec_nid_from_event_slug($event_slug, $season_nid);
  if (!$event_nid){
    _elec_404("Event $event_slug not found for season $season_slug");
  }
  $event_node = node_load(array('nid'=>$event_nid));
  $event_link = _elec_bp() . $_GET['q'];
  $data = array(
    'title' => $event_node->title,
    'link' => $event_link,
    'election_season' => array(
      'title' => $season_node->title,
      'link' => base_path() .  'e/' . $season_slug,
    ),
  );  
  $districts_data = array();
  foreach($event_node->field_elec_participating_distric as $district){
    $district_node = node_load(array('nid' => $district['nid']));
    if(!$district_node) { continue; }
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
  $season_nid = _elec_nid_from_season_slug($season_slug);
  $season_node = node_load(array('nid'=>$season_nid));
  if (!$season_node){
    _elec_404("Season not found");
  }

  $season_link = _elec_bp() . $_GET['q'];


  $election_events = array();
  foreach($season_node->field_elec_election_events as $key => $election_event){
    $e_node = node_load(array('nid'=>$election_event['nid']));
    if(!$e_node) continue; //skip deleted nodes
    $e_event = array(
      'title' => $e_node->title,
      'link'  => $season_link . '/' . _elec_title_to_slug($e_node->title),
    );
    $election_events[] = $e_event;
  }
  $data = array(
    'title' => $season_node->title, 
    'link' =>  $season_link,
    'election_events' => $election_events,
  );
  _elec_write_cache($_GET['q'], $data);
  return $data;
}

function _elec_get_ballot_item($season_slug, $event_slug, $district_slug, $ballot_item_slug){
  $season_nid = _elec_nid_from_season_slug($season_slug);
  if (!$season_nid){
    _elec_404("Season not found");
  }
  $event_nid = _elec_nid_from_event_slug($event_slug, $season_nid);
  if (!$event_nid){
    _elec_404("Event $event_slug not found for season $season_slug");
  }
  $district_nid = _elec_nid_from_district_slug($district_slug, $event_nid);
  if (!$district_nid){
    _elec_404("District $district_slug not found for $event_slug");
  }

  $ballot_item_nid = _elec_nid_from_ballot_item_slug($ballot_item_slug, $district_nid);

  if (!$ballot_item_nid){
    $possible_leaf = false;
    $results = _elec_get_summed_ballot_item($district_nid, $ballot_item_slug, $ballot_item_nid);
    //So we can get the tags
    $ballot_item_node = node_load(array('nid'=>$ballot_item_nid));
    if(!$results){
      _elec_404("$ballot_item_slug not found for district $district_slug");
    }

  } else {
    $possible_leaf = true;
    $ballot_item_node = node_load(array('nid'=>$ballot_item_nid));
    $ballot_item_results = _elec_get_ballot_item_result($ballot_item_nid, $district_nid);
    $winners = _elec_ballot_item_winners($ballot_item_nid, $district_nid, $ballot_item_results);

    $complete =  (_elec_get_ballot_item_is_complete($ballot_item_nid, $district_nid)) ? TRUE : FALSE;
    $results = array();
    if(is_array($ballot_item_results)){
      $total_votes = array_sum($ballot_item_results);

      foreach($ballot_item_results as $key => $option){
        $results[str_replace('_',' ', $key)] = array(
          'votes' => (int)$option,
          'percent' => (100 * round($option / $total_votes, 3)),
          'winner' => in_array($key, $winners),
          'complete' => $complete,
        );
      }
      if(!$complete){
        foreach($results as $key => $val){
          unset($results[$key]['winner']);
          unset($results[$key]['percent']);
        }
      }

    }
  }

  $algo_a = db_fetch_array(db_query('SELECT algorithm, n from {election_algorithm} where ballot_item_nid=%d', $ballot_item_nid));
  $algorithms = _elec_get_winner_algos();
  if( isset($algorithms[$algo_a['algorithm']]) && function_exists($algorithms[$algo_a['algorithm']]['callback']. '_threshold') ){
    //print "the function is " . $algorithms[$algo_a['algorithm']]['callback'] . '_threshold';
    $temp_results = array();
    foreach($results as $key => $val){
      $temp_results[] = $val['votes'];
    }

    $threshold = call_user_func_array($algorithms[$algo_a['algorithm']]['callback'] . '_threshold', array($temp_results, $algo_a['n']));
    foreach ($results as $key => $val){
      $results[$key]['winning_threshold'] = $threshold;
    }
  }


  $leaf_ballot_item = false; //boolean
  $leaf_ballot_item = _elec_is_leaf_ballot_item($ballot_item_nid);

  if($possible_leaf && $leaf_ballot_item) {
    foreach($results as $key => $val){
      unset($results[$key]['winner']);
      unset($results[$key]['percent']);
      unset($results[$key]['complete']);
      unset($results[$key]['winning_threshold']);
    }
  }

  $tags = array();
  foreach($ballot_item_node->taxonomy as $term){
    if($term->vid == variable_get('elec_ballot_vid', NULL)){
      $tags[] = $term->name; 
    }
  }
  
  if(!in_array('Referendum', $tags)){
    //sort ballot items for non-referendums issue #13
    uasort($results, "election_results_compare");
  }

  $district_node = node_load(array('nid'=>$district_nid));
  $district_tags = array();
  foreach($district_node->taxonomy as $term){
    if($term->vid == variable_get('elec_district_vid', NULL)){
      $district_tags[] = $term->name;
    }
  }
  $data = array(
    'title' => $ballot_item_node->title,
    'link' => base_path() . $_GET['q'],
    'election_event' => array(
      'title' => _elec_get_title_from_nid($event_nid),
      'link' => base_path() . 'e/' . $season_slug . '/' . $event_slug,
    ),
    'election_season' => array(
      'title' => _elec_get_title_from_nid($season_nid),
      'link' => base_path() . 'e/' . $season_slug,
    ),
    'district' => array(
      'title' => _elec_get_title_from_nid($district_nid),  
      'link' => base_path() . 'e/' . $season_slug . '/' . $event_slug . '/' . $district_slug,
      'tags' => $district_tags,
    ),
    'tags' => $tags,
    'results' => $results,
  ); 
  _elec_write_cache($_GET['q'], $data);
  return $data;
}

//function for uasort to compare election results
function election_results_compare($a, $b){
  return $b['votes'] -  $a['votes'];
}

function _elec_get_district($season_slug, $event_slug, $district_slug){
  $season_nid = _elec_nid_from_season_slug($season_slug);
  if (!$season_nid){
    _elec_404("Season not found");
  }
  $event_nid = _elec_nid_from_event_slug($event_slug, $season_nid);
  if (!$event_nid){
    _elec_404("Event $event_slug not found for season $season_slug");
  }
  $event_node = node_load(array('nid'=>$event_nid));
  $district_nid = _elec_nid_from_district_slug($district_slug, $event_nid);
  if ($district_nid){
    $district_node = node_load(array('nid'=> $district_nid));
  }

  if(! $district_node){
    _elec_404("Event $district_slug not found for season $season_slug and event $event_slug");
  }
  
  $ballot_item_results = _elec_get_ballot_item_results_by_district($district_nid);
  $summed_results = array();
  foreach($ballot_item_results as $ballot_item){
    foreach($ballot_item as $ballot_item_key => $item) { break; } // this is ugly.  grabs the first item and key from array
    $summed_results[$ballot_item_key]['text'] =  _elec_get_ballot_item_text($ballot_item_key);
    foreach($item as $choice_key => $choice){
     $summed_results[$ballot_item_key]['results'][$choice_key]+= $choice; 
    }
  }

  //Add percentage and link to data
  foreach($summed_results as $key => $ind_result){
    $total_for_ind = array_sum($ind_result['results']);
    $r_w_p = array(); // results with percent   
    foreach($ind_result['results'] as $option => $votes){
      $r_w_p[$option] = array(
        'votes' => $votes,
        'percent' => (100 * round($votes / $total_for_ind, 3)),
      );
    }
    $summed_results[$key]['results'] = $r_w_p;
    $summed_results[$key]['link'] =  '/' . $_GET['q'] . '/' . _elec_slug_from_nid($key);
  }


  $tags = array();
  foreach($district_node->taxonomy as $tag){ $tags[] = $tag->name; }

  $ballot_item_array = array();

  foreach($district_node->field_elec_ballot_items as $ballot_item_nid){
    if (!$ballot_item_nid['nid']) { continue; }
    $ballot_item_node = node_load(array('nid'=>$ballot_item_nid['nid']));
    $ballot_item_row = array();
    $ballot_item_row['title'] = $ballot_item_node->title;
    $ballot_item_row['link'] = base_path() . $_GET['q'] . '/' . _elec_title_to_slug($ballot_item_node->title);
    foreach($ballot_item_node->taxonomy as $ballot_type_term){
      if ($ballot_type_term->vid == variable_get('elec_ballot_vid', NULL)){
        $ballot_item_row['type'] = _elec_swap_plurality(str_replace(' ', '_', $ballot_type_term->name));
        break;
      }
    }
    $ballot_item_array[] = $ballot_item_row;
  }
  $ballot_item_data = array();
  foreach($ballot_item_array as $ballot_item_row){
    $row = array(
      'title' => $ballot_item_row['title'],
      'link' => $ballot_item_row['link'],
    );
    /*$ballot_item_data[$ballot_item_row['type']]['title'] = $ballot_item_row['title']; 
    $ballot_item_data[$ballot_item_row['type']]['link'] = $ballot_item_row['link']; */
    $ballot_item_data[$ballot_item_row['type']][] = $row;
  }

  $districts = array();
  $district_types = array();
  foreach($district_node->field_elec_sub_districts as $sub_district_nid){
    $district_row = array();
    $sub_district_node = node_load(array('nid'=>$sub_district_nid['nid']));
    if(!isset($sub_district_node->taxonomy)) continue;
    foreach($sub_district_node->taxonomy as $sd_term){
      if($sd_term->vid == variable_get('elec_district_vid', NULL)){
        $mr_type = str_replace(' ', '-',_elec_swap_plurality($sd_term->name));
        if(! array_key_exists($mr_type, $district_types) ){
          $district_types[$mr_type] = array();
        }
        $district_row['title'] = $sub_district_node->title;
        $district_row['link'] = base_path() . 'e/' . $season_slug . '/' . $event_slug . '/' . _elec_title_to_slug($sub_district_node->title);
        $districts[$mr_type][] = $district_row;
        //$districts[str_replace(' ', '-',_elec_swap_plurality($sd_term->name))][_elec_title_to_slug($sub_district_node->title)] = $district_row;
      }
    }
  }

  $data = array(
    'title' => $district_node->title,
    'link' => '/' . $_GET['q'],
    'election_event' => array(
      'title' => $event_node->title,
      'link' => base_path() . 'e/' . $season_slug . '/' . $event_slug,
    ),
    'election_season' => array(
      'title' => _elec_get_title_from_nid($season_nid),
      'link' => base_path() . 'e/' . $season_slug,
    ),
    'ballot_items' => $ballot_item_data,
    'ballot_item_results' => $summed_results,
    'tags' => $tags,
    'districts' => $districts,
  );
  _elec_write_cache($_GET['q'], $data);
  return $data;
}

function _elec_get_ballot_item_result($ballot_item_nid, $district_nid){
  $query = 'SELECT results from {election_results} where district_nid = %d AND ballot_item_nid=%d';
  $results = db_result(db_query($query, $district_nid, $ballot_item_nid));
  if($results){
    return unserialize($results);
  }
  return FALSE;
}

function _elec_get_ballot_item_is_complete_recur($ballot_item_nid, $district_nid){
  if(! $district_nid ) return NULL;
  $district_node = node_load(array('nid'=> $district_nid));
  $data = array();
  $query = 'SELECT complete from {election_results} where district_nid = %d AND ballot_item_nid=%d';
  $query_result = db_query($query, $district_nid, $ballot_item_nid);
  $complete = db_result($query_result);
  if($complete === "0"){
    return FALSE;
  }
  foreach($district_node->field_elec_sub_districts as $sub_district){
   if(!$sub_district['nid']) continue;
   if (_elec_get_ballot_item_is_complete_recur($ballot_item_nid, $sub_district['nid']) == FALSE){
     return FALSE;
   }
  }
  return TRUE;
}

function _elec_get_ballot_item_is_complete($ballot_item_nid, $district_nid){
  $query = 'SELECT complete from {election_results} where district_nid=%d AND ballot_item_nid=%d';
  $results = db_result(db_query($query, $district_nid, $ballot_item_nid));
  return $results;
}

/*
 * Recursive function that gets ballot items for the district, and all sub districts
 */

function _elec_get_ballot_item_results_by_district($district_nid){
  if(! $district_nid ) return NULL;
  $district_node = node_load(array('nid'=> $district_nid));
  $data = array();
  $query = 'SELECT ballot_item_nid, results from {election_results} where district_nid = %d';
  $query_result = db_query($query, $district_nid);
  while($ballot_item_result = db_fetch_object($query_result)){
    if ($ballot_item_result){
      $data[] = array($ballot_item_result->ballot_item_nid => unserialize($ballot_item_result->results));
    }
  }
  foreach($district_node->field_elec_sub_districts as $sub_district){
    $children = _elec_get_ballot_item_results_by_district($sub_district['nid']);
    if( ! empty($children) ){
      $data = array_merge($data, $children);
    }
  }
  return $data;
}

function _elec_404($message){
  header('HTTP/1.0 404 Not Found');
  print $message . "\n";
  exit;
}

function _elec_304(){
  header('HTTP/1.0 304 Not Modified');
  exit;
}

function _elec_406(){
  header('HTTP/1.0 406 Not Acceptable');
  exit;
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
  header('Expires: ' . date(Datetime::RFC2822, $expires));
  header('Last-Modified: ' . date(Datetime::RFC2822, $last_modified));
}

function _elec_swap_plurality($string){
  switch(strtolower($string)){
    case 'municipality':
      return 'municipalities';
    case 'municipalities':
      return 'municipality';
    case 'ballot item':
      return 'ballot items';
    case 'ballot items':
      return 'ballot item';
    case 'referendum':
      return 'referendums';
    case 'referendums':
      return 'referendum';
    case 'elections':
      return 'election';
    case 'election':
      return 'elections';
    case 'ward':
      return 'wards';
    case 'polling place':
      return 'polling places';
    case 'polling places':
      return 'polling place';

    default:
      return $string;
  }
}

function _elec_get_title($nid){
  return node_load(array('nid' => $nid))->title;
}

function _elec_get_ballot_item_text($nid){
  $ballot_item_node = node_load(array('nid' => $nid));
  return $ballot_item_node->body;
}


function _elec_find_sub_district($event_nid, $district_slug, $district_nid = NULL){
  if (! $district_nid ){
    $event_node = node_load(array('nid'=>$event_nid)); 
    foreach($event_node->field_elec_participating_distric as $sub_district){
      $district_node = node_load(array('nid' => $sub_district['nid']));
      if ($district_node && $district_node->title == _elec_slug_to_title($district_slug)){
        return $district_node;
      } else{
        $sub_node = _elec_find_sub_district($event_nid, $district_slug, $district_node->nid);
        if($sub_node) { return $sub_node; }
      }
    }
  } 
  else {
    $district_parent = node_load(array('nid'=>$district_nid));
    foreach($district_parent->field_elec_sub_districts as $sub_district){
      if(! $sub_district['nid'] ) { continue; }
      $district_node = node_load(array('nid' => $sub_district['nid']));
      if ($district_node && strtolower($district_node->title) == _elec_slug_to_title($district_slug)){
        return $district_node;
      } elseif($district_node->nid){
        $sub_node =  _elec_find_sub_district($event_nid, $district_slug, $district_node->nid);
        if($sub_node) { return $sub_node; }
      }
    }
  }
  return FALSE;
}


function _elec_nid_from_season_slug($slug){
  return db_result(db_query('SELECT nid FROM {election_slug} WHERE slug="%s" AND type="elec_election_season"', $slug)); 
}

function _elec_slug_from_nid($nid){
  return db_result(db_query('SELECT slug FROM {election_slug} WHERE nid=%d', $nid)); 
}

function _elec_nid_from_event_slug($slug, $season_nid){
  $query = "SELECT es.nid FROM election_slug es " .
           "JOIN content_field_elec_election_events ee ON " .
           "ee.field_elec_election_events_nid = es.nid " .
           "WHERE ee.nid=%d AND es.slug='%s' AND es.type='elec_election_event'";
  return db_result(db_query($query, $season_nid, $slug));
}

function _elec_nid_from_district_slug($slug, $event_nid){
  $query = "SELECT es.nid FROM election_slug es " .
           "JOIN content_field_elec_participating_distric pd ON " .
           "pd.field_elec_participating_distric_nid = es.nid " .
           "WHERE pd.nid=%d AND es.slug='%s' AND es.type='elec_district'";
  $district_nid = db_result(db_query($query,$event_nid, $slug));
  if ($district_nid){
    return $district_nid;
  }
  else{
    $version_id = db_result(db_query('SELECT MAX(vid) from {node} where nid=%d', $event_nid)); 
    $participating_district_query = 'SELECT field_elec_participating_distric_nid FROM {content_field_elec_participating_distric} WHERE vid=%d AND nid=%d';
    $results = db_query($participating_district_query, $version_id, $event_nid);
    while($participating_district_row = db_fetch_object($results)){
      $district_nid = _elec_find_sub_district_recur($slug, $participating_district_row->field_elec_participating_distric_nid);
      if($district_nid){
        return $district_nid;
      }
    }
  }
  return NULL;
}

function _elec_find_sub_district_recur($district_slug, $district_nid){
  $query = "SELECT es.nid FROM election_slug es " .
           "JOIN {content_field_elec_sub_districts} sd ON " .
           "sd.field_elec_sub_districts_nid = es.nid " .
           "WHERE sd.nid=%d AND es.slug='%s' AND es.type='elec_district'";
  $sub_district_nid = db_result(db_query($query, $district_nid, $district_slug));
  if($sub_district_nid){
    return $sub_district_nid;
  }
  else{ //try again, lower
    $version_id = db_result(db_query('SELECT MAX(vid) from {node} where nid=%d', $district_nid)); 
    $sub_district_query = 'SELECT field_elec_sub_districts_nid FROM {content_field_elec_sub_districts} WHERE vid=%d AND nid=%d';
    $results = db_query($sub_district_query, $version_id, $district_nid);
    while($sub_district_row = db_fetch_object($results)){
      $sub_district_nid = _elec_find_sub_district_recur($district_slug, $sub_district_row->field_elec_sub_districts_nid);
      if($sub_district_nid){
        return $sub_district_nid;
      }
    }
  }
  return FALSE;
}
function _elec_get_title_from_nid($nid){
  $version_id = db_result(db_query('SELECT MAX(vid) from {node} where nid=%d', $nid)); 
  return db_result(db_query('SELECT title from {node} where nid=%d AND vid=%d', $nid, $version_id));
}

function _elec_nid_from_ballot_item_slug($ballot_item_slug, $district_nid){
  $query = "SELECT es.nid FROM election_slug es " .
           "JOIN {content_field_elec_ballot_items} bi ON " .
           "bi.field_elec_ballot_items_nid = es.nid " .
           "WHERE bi.nid=%d AND es.slug='%s' AND es.type='elec_ballot_item'";
  $ballot_item_nid = db_result(db_query($query, $district_nid, $ballot_item_slug));
  return $ballot_item_nid;
}

function _elec_remove_csv_extension($s){
  if(substr($s,-4) == '.skl' || substr($s,-4) == '.csv'){
    return substr($s,0,-4);
  } else {
    return $s;
  }
}

// Simply strips the leading forward slash form basepath() 
function _elec_bp(){
 return substr(base_path(),1);
}

function _elec_ballot_item_winners($ballot_item_nid, $district_nid, $ballot_item_results){
  $query = 'SELECT count(*) FROM {election_results} WHERE ballot_item_nid=%d AND district_nid=%d';
  $aggregate = ! (db_result(db_query($query, $ballot_item_nid, $district_nid)));
  if (!$aggregate && !_elec_get_ballot_item_is_complete($ballot_item_nid, $district_nid)){
    return array();
  }
  $algo_a = db_fetch_array(db_query('SELECT algorithm, n from {election_algorithm} where ballot_item_nid=%d', $ballot_item_nid));
  if(!$algo_a) return;
  $algorithms = _elec_get_winner_algos();
  if( isset($algorithms[$algo_a['algorithm']]) && function_exists($algorithms[$algo_a['algorithm']]['callback']) ){
   $winners = call_user_func_array($algorithms[$algo_a['algorithm']]['callback'], array($ballot_item_results, $algo_a['n']));
  }
  return $winners;
  
}


//Returns an array of the indexes of the results array that should be marked winners;
function _elec_algorithm_gt_n($results, $n){
  $winners = array();
  $total_votes = array_sum($results);
  $need_more_than = ($n / 100) * $total_votes;
  foreach($results as $option=>$votes){
    if($votes > $need_more_than){
      $winners[] = $option;
    }
  }
  if (sizeof($winners) > 1){
    $winners = array(max($winners));
  }
  return $winners;
}

function _elec_algorithm_gt_n_threshold($results, $n){
  $winners = array();
  $total_votes = array_sum($results);
  $need_more_than = floor(($n / 100) * $total_votes);
  return $need_more_than + 1;
}

function _elec_algorithm_gte_n($results, $n){
  $winners = array();
  $total_votes = array_sum($results);
  $need_atleast = ceil(($n / 100) * $total_votes);
  foreach($results as $option=>$votes){
    if($votes >= $need_atleast){
      $winners[] = $option;
    }
  }
  if (sizeof($winners) > 1){
    $winners = array(max($winners));
  }
  return $winners;
}

function _elec_algorithm_gte_n_threshold($results, $n){
  $winners = array();
  $total_votes = array_sum($results);
  $need_atleast = ceil(($n / 100) * $total_votes);
  return $need_atleast;
}

function _elec_algorithm_top_n($results, $n){
  $winners = array();
  arsort($results);
  $count = 0;
  foreach($results as $option=>$votes){
    $winners[] =$option;
    ++$count;
    if($count >= $n) break;
  }
  return $winners;
}


function _elec_get_winner_algos(){
  $built_in =  array(
    'gt_n' => array(
      'name' => '> N',
      'id' => 'gt_n',
      'callback' => '_elec_algorithm_gt_n',
    ),
    'gte_n' => array(
      'name' => '>= N',
      'id' => 'gte_n',
      'callback' => '_elec_algorithm_gte_n',
    ),
    'top_n' => array(
      'name' => 'Top N',
      'id' => 'top_n',
      'callback' => '_elec_algorithm_top_n',
    ),
  );
  return $built_in;
}

//ballot item nid is filled in by reference, basically as a hack, as its needed elsewhere
function _elec_get_summed_ballot_item($district_nid, $ballot_item_slug, &$ballot_item_nid){
  $ballot_item_results = _elec_get_ballot_item_results_by_district($district_nid);
  $summed_results = array();
  foreach($ballot_item_results as $ballot_item){
    foreach($ballot_item as $ballot_item_key => $item) { break; } // this is ugly.  grabs the first item and key from array
    $summed_results[$ballot_item_key]['text'] =  _elec_get_ballot_item_text($ballot_item_key);
    foreach($item as $choice_key => $choice){
     $summed_results[$ballot_item_key]['results'][$choice_key]+= $choice; 
    }
  }
  //Add percentage to data
  foreach($summed_results as $key => $ind_result){
    $total_for_ind = array_sum($ind_result['results']);
    $r_w_p = array(); // results with percent   
    foreach($ind_result['results'] as $option => $votes){
      $r_w_p[$option] = array(
        'votes' => $votes,
        'percent' => (100 * round($votes / $total_for_ind, 3)),
      );
    }
    $summed_results[$key]['results'] = $r_w_p;
  }

  foreach($summed_results as $nid => $results){
    $slug = db_result(db_query('SELECT slug from {election_slug} WHERE nid=%d', $nid));
    if($slug == $ballot_item_slug){
      $ballot_item_nid = $nid; // fill in ballot_item_nid
      $complete = _elec_get_ballot_item_is_complete_recur($nid, $district_nid);

      foreach($results['results'] as $key => $option){
        $results_for_winner_detection[$key] = $option['votes'];
      }
      $winners = _elec_ballot_item_winners($ballot_item_nid, $district_nid, $results_for_winner_detection);

      foreach($results['results'] as $key => $option){
        $results['results'][$key]['complete'] = $complete; 
        $results['results'][$key]['winner'] = in_array($key, $winners);
      }

      if(!$complete){
        foreach($results['results'] as $key => $val){
          unset($results['results'][$key]['percent']);
          unset($results['results'][$key]['winner']);

        }
      }

      return $results['results'];
    }
  }
  return null;
}

function _elec_is_leaf_ballot_item($ballot_item_nid){
  $query = 'SELECT COUNT(*) FROM {content_field_elec_ballot_items} WHERE field_elec_ballot_items_nid = %d'; 
  return ( db_result(db_query($query, $ballot_item_nid)) > 1 ) ? true : false;
}


//ballot items passed by reference
function _elec_make_summed_district_data($district_nid, &$ballot_items){
  $summed_ballot_items = _elec_get_ballot_item_results_by_district($district_nid);
  foreach($summed_ballot_items as $ballot_item){
    foreach($ballot_item as $ballot_item_key => $item) { break; } // this is ugly.  grabs the first item and key from array
    $summed_results[$ballot_item_key]['text'] =  _elec_get_ballot_item_text($ballot_item_key);
    foreach($item as $choice_key => $choice){
      $summed_results[$ballot_item_key]['results'][$choice_key]+= $choice; 
    }
  }
  //print "summed ballot items for district $district_slug<pre>"; print_r($summed_ballot_items); print "</pre>";

  //Add percentage to data
  foreach($summed_results as $key => $ind_result){
    $total_for_ind = array_sum($ind_result['results']);
    $r_w_p = array(); // results with percent   
    foreach($ind_result['results'] as $option => $votes){
      $r_w_p[$option] = array(
          'votes' => $votes,
          'percent' => (100 * round($votes / $total_for_ind, 3)),
          );
    }
    $summed_results[$key]['results'] = $r_w_p;
  }

  foreach($summed_results as $nid => $results){
    $slug = db_result(db_query('SELECT slug from {election_slug} WHERE nid=%d', $nid));
    $ballot_item_nid = $nid; // fill in ballot_item_nid
    $complete = _elec_get_ballot_item_is_complete_recur($nid, $district_nid);

    foreach($results['results'] as $key => $option){
      $results_for_winner_detection[$key] = $option['votes'];
    }
    $winners = _elec_ballot_item_winners($ballot_item_nid, $district_nid, $results_for_winner_detection);

    foreach($results['results'] as $key => $option){
      $results['results'][$key]['complete'] = $complete; 
      $results['results'][$key]['winner'] = in_array($key, $winners);
    }

    if(!$complete){
      foreach($results['results'] as $key => $val){
        unset($results['results'][$key]['percent']);
        unset($results['results'][$key]['winner']);

      }
    }
    //print "<pre>results!!!!!!11"; print_r($results); print '</pre>';
    $ballot_items .= _elec_html_ballot_item_skel($results);
  }
}
