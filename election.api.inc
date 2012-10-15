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
        _elec_html_event_skeleton_2($data);
      }else{
        _elec_html_event($data);
      }
      break;
    case 'district':
      _elec_html_district($data);
      break;
    case 'ballot_item':
      if(substr($_GET['q'], -4) == '.skl'){
        _elec_html_ballot_item_wrapper($data);
      }else{
        _elec_html_ballot_item($data);
      }
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
  $season_slug = _elec_title_to_slug($election_season_title);
  $event_slug = _elec_title_to_slug($election_event_title);
  $district_slug = _elec_title_to_slug($district_title);
  $ballot_item_title = ($data->text) ? ($data->text) : $data->title;
  $last_update = strftime('%b %e, %Y %r',mktime());
  $ballot_item_slug = _elec_title_to_slug($ballot_item_title);
  //$data_link = ($data_link_bypass) ? $data_link_bypass : base_path() . 'e/' . $season_slug .'/'. $event_slug . '/' . $district_slug . '/' . $ballot_item_slug;
  $data_link = base_path() . 'e/' . $season_slug .'/'. $event_slug . '/' . $district_slug . '/' . $ballot_item_slug;

  $total_votes = 0;
  $results ="\n\n" . '<article id="' . $district_slug . '-' . $ballot_item_slug . '" class="ballot-item-results ' . strtolower($tag) . '" data-link="' .$data_link . '">' . "\n";
  $results .='  <h1 id="' . $district_slug . '-' . $ballot_item_slug . '-h1' . '">' . $ballot_item_title . '</h1>' . "\n";

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
    // change rounding
    $rounded_raw = round($result['votes'] / $total_votes * 100 , 2 , PHP_ROUND_HALF_DOWN);
    $percent = ($complete) ? number_format($rounded_raw , 2 ) : 'n/a';
    $opt_type = ($type=='election') ? 'candidate' : '';
    $results .= '  <tr class="result ' . $winner . $complete . '">' . "\n";
    $results .= '    <td class="option ' . $opt_type . '"><span class="graph-swatch"></span>' . $option . "</td>\n";
    $results .= '    <td class="votes"> '. $result['votes'] . "</td>\n";
    $footer .='' . "\n";
    $graph .= '<li class="result ' . $opt_type  . '" title="' . $option . ' : ' . $result['votes'] . ' votes, ' 
           . $percent . "%".'" style="width: ' . $percent . "%" . '"> <span class="option">' . $option . '</span> : <span class="votes">' 
           . $result['votes']  . '</span> votes, <span class="percent">' . $percent . '</span>%</li>' . "\n";
    if($result['complete']){ $results .= '    <td class="percent">' .  $percent . "%" . "</td>" . "\n"; }
    $results .= '  </tr>'. "\n";
  }
  $results .= "</tbody>" . "\n" . "</table>" . "\n";
  if ($complete){
    $results .=  $graph . '</ol>' . "\n";
  }else{
    $results .= '<ol class="graph"></ol>' . "\n";
  }
  $results .= _elec_detailed_results_render_articles($data->detailed_results, $opt_type_th);
  $results .= $footer . _elec_html_ballot_item_footer($data) . "\n";
  $results .= '<p>Updated <span class="updated">' . $last_update . '</span></p>' . "\n" . '</footer>' . "\n";
  $results .= '</article>' . "\n";

//print <<<ARTICLE
//$results
//ARTICLE;
  print $results;
}
//preprocess before output?
function _elec_html_skel_wrapper($skel_guts){
  /*<article id="results">
';
  $article = _elec_html_ballot_item($data);
echo '
</article>
 ;*/
}

function _elec_detailed_results_render_articles($detailed_results, $opt_type_th){
  $output = '';
  foreach($detailed_results as $district_nid => $district_result_container){
    $district_node = node_load(array('nid'=>$district_nid));
    $article = '<article>'; 
    $article .= '<h1>' . $district_node->title . '</h1>';
    foreach($district_result_container as $ballot_item_nid => $results); // THIS DOES NOT LOOP
    $article .= "<table><thead><th>$opt_type_th</th><th>Votes</th></thead>";
    foreach($results as $opt => $votes){
      $article .= "<tr><td>$opt</td><td>$votes</td></tr>";
    }
    $article .= '</table>';
    $article .= '</article>';
    $output .= $article . "\n";
  }
  return $output;
}

//This takes the detailed_results from _elec_get_ballot_item as generates an
//html table of the results.
function _elec_detailed_results_render_table($detailed_results, $opt_type_th){
  if(!$detailed_results){ return null; }
  $ths = array();
  $ths[] = $opt_type_th;
  $choices = array(); 
  $vote_grid = array();
  //Load up the choices and wards for use in the table.
  foreach($detailed_results as $district_nid => $district_result_container){
    $district_node = node_load(array('nid'=>$district_nid));
    $ths[] = $district_node->title;
    foreach($district_result_container as $ballot_item_nid => $results); // THIS DOES NOT LOOP
    if($choices === array()){
      $choices = _elec_load_detailed_results_choices($results);
    }
  }

  //Build the grid of votes to render the table
  foreach($detailed_results as $district_nid => $district_result_container){
    foreach($district_result_container as $ballot_item_nid => $results); // THIS DOES NOT LOOP
    foreach($results as $choice => $votes){
      $vote_grid[$choice][] = $votes;
    }
  }

  //Render table
  $table = "<table>\n\t<thead>\n";
  foreach($ths as $th){ $table .= "<th>$th</th>"; }
  $table .= "</thead>";
  foreach($vote_grid as $choice => $votes){
    $table .= "<tr>";
    $table .= "  <td>$choice</td>"; 
    foreach($votes as $vote_num){
      $table .= "<td>$vote_num</td>";
    }
    $table .= "</tr>";
  }
  $table .= "</table>";
  return $table;
}


function _elec_load_detailed_results_choices($results){
  foreach($results as $choice => $votes){
    $choices[] = $choice;    
  }
  return $choices;
}


/* Makes an updating html page out of a API call for flat html
(for use on the Studio Laptop output to the switcher on TV) currently called with
.skl appened to ballot item in API */
function _elec_html_ballot_item_wrapper($data){
  echo '
<!DOCTYPE html>
<head>
  <script type="text/javascript" src="https://www.cctv.org/sites/all/themes/zen/cctv/js/html5shiv.js"></script>
  <script type="text/javascript" src="https://www.cctv.org/sites/all/themes/zen/cctv/js/html5shiv-printshiv.js"></script>
  <script type="text/javascript" src="https://www.cctv.org/sites/all/themes/zen/cctv/js/jquery-1.7.1.min.js"></script>
  <script type="text/javascript" src="https://www.cctv.org/sites/all/themes/zen/cctv/js/jquery.format-1.2.min.js"></script>
  <script type="text/javascript" src="https://www.cctv.org/sites/all/themes/zen/cctv/js/jquery.ba-hashchange.min.js"></script>
  <script type="text/javascript" src="https://www.cctv.org/sites/all/themes/zen/cctv/js/jquery.smooth-scroll.min.js"></script>
  <script type="text/javascript" src="https://www.cctv.org/sites/all/themes/zen/cctv/js/ballot-item-results.js"></script>
  <script type="text/javascript" src="https://www.cctv.org/sites/all/themes/zen/cctv/js/jquery-noconflict.js"></script>
  <link type="text/css" rel="stylesheet" media="all" href="https://www.cctv.org/sites/all/themes/zen/cctv/electioncss/reset.css" />
  <link type="text/css" rel="stylesheet" media="all" href="https://www.cctv.org/sites/all/themes/zen/cctv/electioncss/layout.css" />
  <link type="text/css" rel="stylesheet" media="all" href="https://www.cctv.org/sites/all/themes/zen/cctv/electioncss/ballot-item-results.css" />
</head>
<body>
<article id="results">
';
  $article = _elec_html_ballot_item($data);
echo '
</article>
</body>
</html>
';
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

function _elec_html_ballot_item_skel($data, $data_link_bypass = NULL){
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
  $data_link = ($data_link_bypass) ? $data_link_bypass : base_path() . 'e/' . $season_slug .'/'. $event_slug . '/' . $district_slug . '/' . $ballot_item_slug;

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
    // change rounding
    $rounded_raw = round($result['votes'] / $total_votes * 100 , 2 , PHP_ROUND_HALF_DOWN);
    $percent = ($complete) ? number_format($rounded_raw , 2 ) : 'n/a';
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
  //print_r($data);
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

function _elec_split_combined_slug($combined_district_bi_slug){
      $out_arr = array();
      $u_slugs_arr = explode("/", $combined_district_bi_slug);
      $out_arr['district_slug'] = $u_slugs_arr[0];
      $out_arr['ballot_item_slug'] = $u_slugs_arr[1];
      return $out_arr;
}

/*builds the new HTML skeleton and nav interface for the results page -
we need to be able to call this function with the election event $data.
This is a very heavy function and so the final call dumps it to a
uniquely named .php file based on the election event nid. Inside it is
just the string variable storing all the results and a print command.
It is then included as a php file (via nid to reconstruct filename) from the file cache
into the theme (node-elec_election_event.tpl.php), where it evaluates and prints.  -AC*/
function _elec_html_event_skeleton_2($data){
  //print "<pre>"; print_r($data); print "</pre>";
  $master_ballot_tree = array();
  $data = (object)$data;
  $election_nav = ''; /* start collect html for nav */
  $ballot_items = ''; /* start collect html for results */
  $season_slug = _elec_last_uri_path($data->election_season['link']);
  $season_nid = _elec_nid_from_season_slug($season_slug);
  $event_slug = _elec_last_uri_path(substr($_GET['q'], 0, -4));
  $event_nid = _elec_nid_from_event_slug($event_slug, $season_nid);
  $district_type_containers = array_slice((array)$data, 3, NULL, true); //remove title, link, election-season -GG
  $dafault_data_struct = array();
  //build initial data structure 
  foreach($district_type_containers as $key => $container){
    //loops through "wards", "municipalities" other tags
    foreach($container as $e_event){
      //loops through districts
      $district_slug = _elec_title_to_slug($e_event['title']);
      $district_nid = _elec_nid_from_district_slug($district_slug, $event_nid);
      $ballot_tree = _elec_build_ballot_item_tree_at_district($district_nid);
      //build tree through districts by building list by ballot item and assoc districts
      $default_data_struct[$district_nid] = array('slug'=>$district_slug, 'title'=>$e_event['title'], 'ballot_items'=>$ballot_tree);
      //print_r($ballot_tree);
    }
  }
  /* we need to get ballot items with results that are spread
  out over parent trees that may have no common parent district */
  //print '<pre>'; print_r($default_data_struct); print '</pre>';
  $base_bi_district = array();
  $parent_bi_district_list = array();
  foreach($default_data_struct as $district_nid_key){
    //loops initial district level unique districts
    $district_slug = $district_nid_key['slug'];
    $district_nid = _elec_nid_from_district_slug($district_slug, $event_nid);
    $bi_exist_top = _elec_dist_has_ballot_items_at_top($district_nid);
    $subdist_exist = _elec_subdistricts_exist($district_nid);       
    foreach($district_nid_key['ballot_items'] as $ballot_item){
      //loops ballot items at district level
      $ballot_item_slug = $ballot_item['slug'];
      $ballot_item_nid = $ballot_item['nid'];
      //collect list of base only ballot items.
      if($bi_exist_top['bi_top'] == "yes"){
        foreach($ballot_item['districts'] as $sub_district){
          if($sub_district['district_nid'] == $district_nid){
            $sub_district_slug = _elec_title_to_slug($sub_district['district_title']);
            $base_bi_list[$sub_district_slug . "/" . $ballot_item_slug] = array('combined_slug'=>$sub_district_slug . "/" . $ballot_item_slug, 'district_slug'=>$sub_district_slug, 'ballot_item_slug'=>$ballot_item_slug);
          }            
        }
      }
      // collect a list of parent only ballot items.
      if($bi_exist_top['bi_top'] == "no"){
        foreach($ballot_item['districts'] as $sub_district){
          if($sub_district['district_slug'] != $district_nid){
            $sub_district_slug = _elec_title_to_slug($sub_district['district_title']);
            $parent_bi_district_list[$district_slug . "/" . $ballot_item_slug] = array('combined_slug'=>$district_slug . "/" . $ballot_item_slug, 'district_slug'=>$district_slug, 'ballot_item_slug'=>$ballot_item_slug);
          }             
        }
      }

    } 
  }
  //combine base only ballot items and parent only ballot items into master ballot item list
  foreach($parent_bi_district_list as $parent_slugs){
    foreach($base_bi_list as $base_slugs){
      if ($parent_slugs['ballot_item_slug'] == $base_slugs['ballot_item_slug']){
        if (array_key_exists($parent_slugs['combined_slug'], $master_ballot_tree)  == FALSE){
          $master_ballot_tree[$parent_slugs['combined_slug']] = array('summed_slug'=>$parent_slugs['combined_slug'], 'base_slugs'=>array( array('base_slug'=>$base_slugs['combined_slug']))); 
        }else{
          $add_base_slug = array('base_slug'=>$base_slugs['combined_slug']);
          array_push($master_ballot_tree[$parent_slugs['combined_slug']]['base_slugs'], $add_base_slug);
        }
      }else{
        $master_ballot_tree[$base_slugs['combined_slug']] = array('summed_slug'=>$base_slugs['combined_slug'], 'base_slugs'=>array( NULL ));  
      }
    }
  }
  ksort($master_ballot_tree, SORT_REGULAR);
  print '<pre>'; print_r($master_ballot_tree); print '</pre>';

  /* below is customized for each type of election event and can be handled as a separate function call, 
     but it must pass the season and event slug variables through so you can get the proper ballot item html*/

  //build html out from master ballot tree.
  foreach($master_ballot_tree as $unique_dist_bi_slug){
    $u_slugs_arr = _elec_split_combined_slug($unique_dist_bi_slug['summed_slug']);
    /* this skips duplicate ward listings with identical results at the base level. 
       HTML results are only printed out from the higher level district */ 
    if( ! ($u_slugs_arr['district_slug'] == "ward-1" 
            || $u_slugs_arr['district_slug'] == "ward-2" || $u_slugs_arr['district_slug'] == "ward-3" 
            || $u_slugs_arr['district_slug'] == "ward-4" || $u_slugs_arr['district_slug'] == "ward-5" 
            || $u_slugs_arr['district_slug'] == "ward-6" || $u_slugs_arr['district_slug'] == "ward-7"
          )
      )
    {  
      $ballot_items_data = _elec_get_ballot_item($season_slug,$event_slug,$u_slugs_arr['district_slug'],$u_slugs_arr['ballot_item_slug']);
      $ballot_items .= _elec_html_ballot_item($ballot_items_data);
    }
    if(isset($unique_dist_bi_slug['base_slugs']['0'])){
      foreach($unique_dist_bi_slug['base_slugs'] as $ballot_contributor){
        //var_dump($ballot_contributor);
        $u_slugs_arr_sub = _elec_split_combined_slug($ballot_contributor['base_slug']);
        //echo $u_slugs_arr_sub['district_slug'] . " " . $u_slugs_arr_sub['ballot_item_slug'];
        $ballot_items_data_sub = _elec_get_ballot_item($season_slug,$event_slug,$u_slugs_arr_sub['district_slug'],$u_slugs_arr_sub['ballot_item_slug']);
        $ballot_items .= _elec_html_ballot_item($ballot_items_data_sub);
     }
    }
  }
 //print '<pre>'; print_r($master_ballot_tree); print '</pre>';
  $ballot_items .= "\n";/*end results html*/
  $election_nav .= "\n";/*end nav html*/
  $elec_results = '<div id ="nav_wrapper" class="election-event-nav">' . "\n" . $election_nav . "\n" . '</div>' . "\n" . '<div id="results_wrapper"  class="election-event-results">' . "\n" . $ballot_items . "\n" . '</div>';
  print $elec_results;
  //_elec_html_skel_to_file($event_nid,$ballot_items,$election_nav);
  //exit;
}

/*builds the HTML skeleton and nav interface for the results page -
we need to be able to call this function with the election event $data.
This is a very heavy function and so the final call dumps it to a
uniquely named .php file based on the election event nid. Inside it is
just the string variable storing all the results and a print command.
It is then included as a php file (via nid to reconstruct filename) from the file cache
into the theme (node-elec_election_event.tpl.php), where it evaluates and prints.  -AC*/

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
  $election_nav = ''; //collect html for nav
  $ballot_items = '';   //collect html for results
  foreach($district_type_containers as $key => $container){
    //only use municipalities.
    if($key == "municipalities"){   
    foreach($container as $e_event){
      $district_slug = _elec_title_to_slug($e_event['title']);
      $district_nid = _elec_nid_from_district_slug($district_slug, $event_nid);
      $election_nav .= '<li><a href="#' . $district_slug . '">' . $e_event['title'] . '</a>';/* district nav */
      $ballot_items .= '<article id="' . $district_slug . '" class="district-results ' .  strtolower(_elec_swap_plurality($key)) . '">' . "\n" . '<h1><a href="#' . $district_slug . '">' . $e_event['title'] . '</a></h1>' . '<!--4o-->' . "\n";/* district header */
      //<a href="#city-of-burlington">City of Burlington</a>
      //$district_data = _elec_get_ballot_item_results_by_district($district_nid);
      $district_data = _elec_get_district($season_slug, $event_slug, $district_slug);
      /*$district_data = _  need burlington sub districts branch? summed */
      
      $district_data = (object)$district_data;
      if(empty($district_data->districts)){
        $election_nav .= '</li>' . "\n";/* district nav for when final recursion hit */
        //$ballot_items .= '</article>' . '<!--new-->' . "\n";
      }

//start of ugliness
      if(! empty($district_data->districts)){
        //SCARY BADNESS!!!!!!!!!!!!!  This thing maniuplates $ballot_items
        $referendums = array();
        $elections = array();
        _elec_make_summed_district_data($district_nid, $ballot_items, $referendums, $elections);

        $ballot_items .= '<article style="display: block;"><h1>Elections</h1>';
        foreach($elections as $election){
          $ballot_items .= _elec_html_ballot_item_skel($election['results'], $election['data_link']);
        }
        $ballot_items .= '</article>';

        $ballot_items .= '<article style="display: block;"><h1>Referendums</h1>';
        foreach($referendums as $referendum){
          $ballot_items .= _elec_html_ballot_item_skel($referendum['results'], $referendum['data_link']);
        }
        $ballot_items .= '</article>';

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
      
      //$election_nav .= '</li>' . "\n";/* district nav for when final recursion hit */
      foreach ($district_data->ballot_items as $ballot_item_type => $ballot_item_list){
        /*$election_nav .= '<a href="#' . $e_event['title'] . ' ' . ucfirst($ballot_item_type) . '">' . ucfirst($ballot_item_type) . '</a>' . "\n"; ballot-item tag nav */
        $ballot_items .= '<article>' . "\n" . '<h1>' . ucfirst($ballot_item_type) . '</h1>' . '<!--3o-->' . "\n";/* district ballot-item type anchor and title */

        foreach($ballot_item_list as $ballot_item){

          $ballot_item_slug = _elec_title_to_slug($ballot_item['title']);
          $ballot_items_data = _elec_get_ballot_item($season_slug,$event_slug,$district_slug,$ballot_item_slug);
          //print "ballot item data:<pre>"; print_r($ballot_items_data); print "</pre>";
          $ballot_items .= _elec_html_ballot_item_skel($ballot_items_data);
        }/*ballot item loop*/
        $ballot_items .= '</article>' . '<!--6c-->' .  "\n"; //here2
      }/*ballot item type loop*/
      if(empty($district_data->districts)){
        $ballot_items .= '</article>' . '<!--new-->' . "\n";
      }
    }/*district loop*/
    $ballot_items .= '</article>' . '<!--5c-->' .  "\n"; /*end municipality loop*/
    }/* endif to do just municipalities*/
 
  }/*district type loop*/
  $ballot_items .= "\n";/*end results html*/
  $election_nav .= "\n";/*end nav html*/
  $elec_results = '<div id ="nav_wrapper" class="election-event-nav">' . "\n" . $election_nav . "\n" . '</div>' . "\n" . '<div id="results_wrapper"  class="election-event-results">' . "\n" . $ballot_items . "\n" . '</div>';
  print $elec_results;
  _elec_html_skel_to_file($event_nid,$ballot_items,$election_nav);
  exit;
};

function _elec_skel_eat_subdistricts($sub_districts, $event_nid, $season_slug, $event_slug,  &$ballot_items, &$election_nav, $depth = 1){
  if(empty($sub_districts) ) return;
  $ballot_items .= '<article><h1>Sub-Districts</h1>';
  $election_nav .= '<ol>' . "\n";
  //print "(depth=$depth)sub distrcts:<pre>"; print_r($sub_districts); print "</pre>";
  foreach($sub_districts as $key => $container){

    foreach($container as $e_event){
      $district_slug = _elec_title_to_slug($e_event['title']);
      $district_nid = _elec_nid_from_district_slug($district_slug, $event_nid);
      $election_nav .= '<li><a href="#' . $district_slug . '">' . $e_event['title'] . '</a>';/* district nav */
      $ballot_items .= '<article id="' . $district_slug . '" class="district-results ' . strtolower(_elec_swap_plurality($key)) .'">' . "\n" . '<h1><a href="#' . $district_slug . '">' . $e_event['title'] . '</a></h1>' . '<!--2o-->' . "\n";/* district header */
      //$district_data = _elec_get_ballot_item_results_by_district($district_nid);
      $district_data = _elec_get_district($season_slug, $event_slug, $district_slug);
      $summed_ballot_items = _elec_strip_district_layer(_elec_get_ballot_item_results_by_district($district_nid));
      //print "calling with district nid $district_nid ($district_slug) </br>";
      //print "summed ballot items:<pre>"; print_r($summed_ballot_items); print "</pre>";
      /*$district_data = _  need burlington sub districts branch? summed */
      $district_data = (object)$district_data;
      if(empty($district_data->districts)){
        $election_nav .= '</li>' . "\n";/* district nav for when final recursion hit */
        //$ballot_items .= '</article>' . '<!--2c-->' . "\n";
      }

//start of ugliness
      if(! empty($district_data->districts)){
        //SCARY BADNESS!!!!!!!!!!!!!  This thing maniuplates $ballot_items
        $referendums = array();
        $elections = array();
        _elec_make_summed_district_data($district_nid, $ballot_items, $referendums, $elections);

        $ballot_items .= '<article style="display: block;"><h1>Elections</h1>';
        foreach($elections as $election){
          $ballot_items .= _elec_html_ballot_item_skel($election['results'], $election['data_link']);
        }
        $ballot_items .= '</article>';

        $ballot_items .= '<article style="display: block;"><h1>Referendums</h1>';
        foreach($referendums as $referendum){
          $ballot_items .= _elec_html_ballot_item_skel($referendum['results'], $referendum['data_link']);
        }
        $ballot_items .= '</article><!--end ref-->';

        $ballot_items .= '</article>';
      }
      //$ballot_items .= '</article><!--end ward cap2-->';
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
      //$election_nav .= '</li>' . "\n";/* district nav for when final recursion hit */
      foreach ($district_data->ballot_items as $ballot_item_type => $ballot_item_list){
        /*$election_nav .= '<a href="#' . $e_event['title'] . ' ' . ucfirst($ballot_item_type) . '">' . ucfirst($ballot_item_type) . '</a>' . "\n"; ballot-item tag nav */
        /*$ballot_items .= "\n" . '<h1 id="' . $e_event['title'] . ' ' . ucfirst($ballot_item_type) . '">' . ucfirst($ballot_item_type) . '</h1>' . "\n"; district ballot-item type anchor and title */
        $ballot_items .= '<article>' . "\n" . '<h1>' . ucfirst($ballot_item_type) . '</h1>' . '<!--1o-->' .  "\n";/* district ballot-item type anchor and title */
        foreach($ballot_item_list as $ballot_item){

          $ballot_item_slug = _elec_title_to_slug($ballot_item['title']);
          $ballot_items_data = _elec_get_ballot_item($season_slug,$event_slug,$district_slug,$ballot_item_slug);
          $ballot_items .= _elec_html_ballot_item_skel($ballot_items_data);

        }/*ballot item loop*/
        $ballot_items .= '</article>'. '<!--1c-->' . "\n"; //here
      }/*ballot item type loop*/
      $ballot_items .= '</article>' . '<!--2c-->' . "\n";
    }/*district loop*/
    $ballot_items .= '</article>' . '</article>' . '<!--wardsend-->' . '<!--burlingtonend-->' . "\n";
  }
  $election_nav .= '</ol>' . "\n" . '</li>' . "\n";
  //$ballot_items .= '</article>' . '<!--4c-->' . "\n";
}

/* result HTML text to a php file, clobbers old file, likely a big potential security risk writing a php file in files/election */
function _elec_html_skel_to_file($event_nid,$elec_results,$elec_nav){
  $file = file_directory_path() . '/' . 'election' . '/' . strval($event_nid) . '-elec-skeleton' . '.php';
  /*$elec_results =  "here is some cached HTML skeleton from the election module";*/
  $output = '<?php $results = \'' . $elec_results . '\'; ' . "\n" . '$elec_nav = \'' . $elec_nav . '\'; ' . "\n" . 'print $results; ?>';
  /*print($file);*/
  $filehandle = fopen( $file , "w");
  fwrite($filehandle, $output);
  fclose($filehandle);
};



function _elec_as_csv($data,$type){
  $filename = str_replace('/','_', $_GET['q']);
  if($type != 'ballot_item' && $type != 'district'){
    _elec_406();
  }
  switch($type){
    case 'district': $output = _elec_district_csv($data);
      break;
    case 'ballot_item' : $output = _elec_ballot_item_csv($data);
      break;
  }
  if($type == 'district'){
  }
  header('Content-Disposition: attachment; filename="'. $filename  . '"');
  $outstream = fopen("php://output",'w'); 
  foreach($output as $row){
    fputcsv($outstream, $row, ',', '"');
  }
  fclose($outstream);
  exit;
}


function _elec_district_csv($data){
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
  return $output;
}

function _elec_ballot_item_csv($data){
  print_r($data);
  return $output;
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
  return $output;
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
    $summed_results = _elec_get_summed_ballot_item($district_nid, $ballot_item_slug, $ballot_item_nid);
    $results = $summed_results['results'];
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
  $data['detailed_results'] = $summed_results['detailed_results'];
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
  
  $ballot_item_results = _elec_strip_district_layer(_elec_get_ballot_item_results_by_district($district_nid));
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
  $query = 'SELECT ballot_item_nid, results, district_nid from {election_results} where district_nid = %d';
  $query_result = db_query($query, $district_nid);
  while($ballot_item_result = db_fetch_object($query_result)){
    if ($ballot_item_result){
      $data[] = array('district_nid'=>$district_nid, 'results'=> array($ballot_item_result->ballot_item_nid => unserialize($ballot_item_result->results)));
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

/* helper function to massage data to format expected,
  we changed _elec_get_ballot_item_results_by_district to return
  {district: nid, data : {..thedata} }
  rather than simply
  {..thedata}
  this data structure is 'assumed' in other parts of the code, and this
  function changes it to the assumed format
  */

function _elec_strip_district_layer($data){
  $ret = array();
  foreach($data as $item){
    $ret[] = $item['results'];
  }
  return $ret;
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
    case 'wards':
      return 'ward';
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

//returns array
function _elec_dist_has_ballot_items_at_top($district_nid){
  $result = array();
  $dist_result = array();
  $bi_exist_top = array();
  $query = 'SELECT `content_field_elec_ballot_items`.`nid`,`content_field_elec_ballot_items`.`field_elec_ballot_items_nid` ' . 
           'FROM `content_field_elec_ballot_items`' .
           'WHERE `content_field_elec_ballot_items`.`nid` = %d ' .
           'AND `content_field_elec_ballot_items`.`field_elec_ballot_items_nid` IS NOT NULL';
  $result = db_query($query, $district_nid);
  for ($row; $row = db_fetch_array($result); $dist_result[] = $row); 
  $num_results = count($dist_result);
  //echo "#bi: ";
  //var_dump($num_results);
  if($num_results > 0){
    $bi_exist_top['bi_top'] = "yes";
    $bi_exist_top['bi_top_count'] = $num_results;
    $bi_exist_top['result'] = $dist_result;
  }else{
    $bi_exist_top['bi_top'] = "no";
    $bi_exist_top['bi_top_count'] = NULL;
    $bi_exist_top['result'] = NULL;
  }
  return $bi_exist_top;
}

//returns array.
function _elec_subdistricts_exist($district_nid){
  $result = array();  
  $subdist_result = array();
  $subdist_exist = array();
  $query = 'SELECT `content_field_elec_sub_districts`.`nid`, `content_field_elec_sub_districts`.`field_elec_sub_districts_nid` ' .
           'FROM `content_field_elec_sub_districts` ' .
           'WHERE `content_field_elec_sub_districts`.`nid` = %d ' . 
           'AND `content_field_elec_sub_districts`.`field_elec_sub_districts_nid` IS NOT NULL ';
  $result = db_query($query, $district_nid);
  for ($row; $row = db_fetch_array($result); $subdist_result[] = $row);
  $num_results = count($subdist_result);
  //echo "#sd: ";
  //var_dump($num_results);
   if($num_results > 0){
     $subdist_exist['sd_exist'] = "yes";
     $subdist_exist['sd_count'] = $num_results;
     $subdist_exist['result'] = $subdist_result;
   }else{
     $subdist_exist['sd_exist'] = "no";
     $subdist_exist['sd_count'] = NULL;
     $subdist_exist['result'] = NULL;
   }
  return $subdist_exist;
}
// check case? move to switch? 
function _elec_build_ballot_item_tree_at_district($district_nid){
  $result = array();
  $dist_slug = _elec_slug_from_nid($district_nid);
  $subdist_exist = _elec_subdistricts_exist($district_nid);
  $bi_exist_top = _elec_dist_has_ballot_items_at_top($district_nid);
  //echo "ballot item exists at top level: " . $bi_exist_top . " *";
  //get the proper query to produce the district ballot-item pairs at the district level
  if ($bi_exist_top['bi_top'] == "no" && $subdist_exist['sd_exist'] == "yes"){
  //case no top ballot items and has Subdistricts: (City of Burlington) summed walk required nested HTML output
  // works
  $query = 'SELECT `content_field_elec_ballot_items`.`nid`, `content_field_elec_ballot_items`.`field_elec_ballot_items_nid` ' .
           'FROM `content_field_elec_ballot_items` , `content_type_elec_district`, `content_field_elec_sub_districts` ' .
           'WHERE `content_type_elec_district`.`nid` = %d ' .
           'AND `content_field_elec_sub_districts`.`field_elec_sub_districts_nid` = `content_field_elec_ballot_items`.`nid` ';
  }
  if ($bi_exist_top['bi_top'] == "yes" && $subdist_exist['sd_exist'] == "yes"){
  // case has top ballot items and has Subdistricts: (Chittenden County) summed walk required nested HTML output
  $query = 'SELECT `content_field_elec_ballot_items`.`nid`, `content_field_elec_ballot_items`.`field_elec_ballot_items_nid` ' .
           'FROM `content_field_elec_ballot_items` , `content_type_elec_district` , `content_field_elec_sub_districts` ' . 
           'WHERE `content_type_elec_district`.`nid` = %d ' .
           'AND (' .
           '`content_field_elec_sub_districts`.`field_elec_sub_districts_nid` = `content_field_elec_ballot_items`.`nid` ' .
           'OR `content_field_elec_ballot_items`.`nid` = %d ' .
           ')'.
           'AND `content_field_elec_ballot_items`.`field_elec_ballot_items_nid` IS NOT NULL ';
  }
  if ($bi_exist_top['bi_top'] == "yes" && $subdist_exist['sd_exist'] == "no"){
  // case has top ballot items and no Subdistricts: (Ward 1, Town of Williston) - default HTML output
  // works
  $query = 'SELECT `content_field_elec_ballot_items`.`nid`, `content_field_elec_ballot_items`.`field_elec_ballot_items_nid` ' .
           'FROM `content_field_elec_ballot_items` ' .
           'WHERE `content_field_elec_ballot_items`.`nid` = %d ';
  }
  // end node district, contains no results by definition
  //if ($bi_exist_top == "no" && $subdist_exist == "no"){
  //$query = '';
  //echo "Warning: District with no ballot items and no subdistricts. ";
  //}
  $result = db_query($query, $district_nid);
  for ($row; $row = db_fetch_array($result); $ballot_dist[] = $row);
  //print_r($result);
  //print_r($ballot_dist);
  $ballot_tree = array();
  //walk the flat list and build tree for ballot items based on ballot item nid
  foreach($ballot_dist as $row){
    $bi_key = $row['field_elec_ballot_items_nid'];
    $dist_nid = $row['nid'];
    $bt_dist_title = _elec_get_title_from_nid($dist_nid);
    $bt_bitem_title = _elec_get_title_from_nid($bi_key);
    if(array_key_exists($bi_key, $ballot_tree) == FALSE ){
      // create a base record for the ballot item
      $ballot_tree[$bi_key] = array('nid'=>$bi_key, 'title'=>$bt_bitem_title, 'slug'=>_elec_slug_from_nid($bi_key), 'districts'=>array( array('district_nid'=>$dist_nid, 'district_title'=>$bt_dist_title, 'results'=>_elec_get_ballot_item_result($bi_key, $dist_nid))));
    }else{
      // add another entry to the district array of an already existing record
      $add_district_arr = array('district_nid'=>$dist_nid, 'district_title'=>$bt_dist_title, 'results'=>_elec_get_ballot_item_result($bi_key, $dist_nid));
      array_push($ballot_tree[$bi_key]['districts'],$add_district_arr);
    }
  }
  //print_r($ballot_tree);
  return $ballot_tree;
}

// just returns one branch from the ballot item tree. Data struture is slightly redundant
// with nid because internal array pointer loses key value when it iterates on it
// in foreach loop (i think)?
function _elec_get_ballot_from_ballot_tree($ballot_tree,$ballot_item_slug){
  foreach($ballot_tree as $bi){
    if ($bi['slug'] == $ballot_item_slug){
      $bi_result[$bi['nid']] = $bi;
    }
  }
  //print_r($bi_result);
  return $bi_result;
}

//ballot item nid is filled in by reference, basically as a hack, as its needed elsewhere
function _elec_get_summed_ballot_item($district_nid, $ballot_item_slug, &$ballot_item_nid){
  $results_with_dd = _elec_get_ballot_item_results_by_district($district_nid); //results with district data
  $ballot_item_results = _elec_strip_district_layer($results_with_dd);
  //echo "target ballot item nid is: " . $ballot_target_nid . " ";
  //echo "target district viewed is: " . $district_nid . " ";
  //print_r($results_with_dd);
  $dd_arr=array();
  /*build the district list with ballot_item data just for ballot item called*/
  foreach($results_with_dd as $district_key){
    $district_nid = $district_key['district_nid'];
    $district_title = _elec_get_title_from_nid($district_nid);
    $ballot_result_nid = key($district_key['results']);
    $ballot_result_title = _elec_get_title_from_nid($ballot_result_nid);
    $results = $district_key['results'];
    if ($ballot_target_nid == $ballot_result_nid){
      $dd_arr[] = array('district_nid'=>$district_nid, 'district_title'=>$district_title, 'ballot_result_nid'=>$ballot_result_nid, 'ballot_result_title'=>$ballot_result_title, 'results'=>$results);
    }
  }
  /* now we have two structures with subdistrict results..
  echo "------------------\n";
  print_r($bi_result);
  echo "------------------\n";
  print_r($dd_arr); 
  echo "------------------\n";  
  */

  //begin normal execution route with summed results..
  $summed_results = array();
  foreach($results_with_dd as $ballot_item){
    $district_nid = $ballot_item['district_nid'];
    unset($ballot_item['district_nid']);
    foreach($ballot_item['results'] as $ballot_item_key => $item) { break; } // this is ugly.  grabs the first item and key from array
    //---------foreach loop above breaks immediately!------------//  first item is the results for a ballot item at $district_nid
    $summed_results[$ballot_item_key]['text'] =  _elec_get_ballot_item_text($ballot_item_key);
    foreach($item as $choice_key => $choice){
     $summed_results[$ballot_item_key]['results'][$choice_key]+= $choice; 
    }
    $summed_results[$ballot_item_key]['detailed_results'][$district_nid] = $ballot_item['results'];
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

      return $results;
    }
  }
  return null;
}

function _elec_is_leaf_ballot_item($ballot_item_nid){
  $query = 'SELECT COUNT(*) FROM {content_field_elec_ballot_items} WHERE field_elec_ballot_items_nid = %d'; 
  return ( db_result(db_query($query, $ballot_item_nid)) > 1 ) ? true : false;
}


//ballot items passed by reference
function _elec_make_summed_district_data($district_nid, &$ballot_items, &$referendums, &$elections){
  $summed_ballot_items = _elec_strip_district_layer(_elec_get_ballot_item_results_by_district($district_nid));
  foreach($summed_ballot_items as $ballot_item){
    foreach($ballot_item as $ballot_item_key => $item) { break; } // this is ugly.  grabs the first item and key from array
    $ballot_item_node = node_load(array('nid'=>$ballot_item_key));
    $summed_results[$ballot_item_key]['link'] = $district_link;
    $summed_results[$ballot_item_key]['text'] =  _elec_get_ballot_item_text($ballot_item_key);
    $summed_results[$ballot_item_key]['title'] =  $ballot_item_node->title; 
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
    $district_link = substr($_GET['q'], 0, -4);
    $ballot_item_node = node_load(array('nid'=>$nid));
    //print "<pre>"; print_r($ballot_item_node->taxonomy);  print "</pre>";
    $vocab = variable_get('elec_ballot_vid', NULL); 
    $district_node = node_load(array('nid'=>$district_nid));
    $data_link = base_path() . $district_link . '/' . _elec_title_to_slug($district_node->title) . '/' . _elec_title_to_slug($ballot_item_node->title);
    foreach($ballot_item_node->taxonomy as $term){
      if ($term->vid == $vocab){
        if($term->name == 'Election'){
          $elections[] = array(
              'results' => $results,
              'data_link' => $data_link,
              );
        } elseif($term->name == 'Referendum'){
          $referendums[] = array(
              'results' => $results,
              'data_link' => $data_link,
              );   
        }
      }
    }

     // $ballot_items .= _elec_html_ballot_item_skel($results, $data_link);
  }
}
