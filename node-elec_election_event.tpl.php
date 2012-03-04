<?php
// $Id: node.tpl.php,v 1.4 2008/09/15 08:11:49 johnalbin Exp $

/**
 * @file node.tpl.php
 *
 * Theme implementation to display a node.
 *
 * Available variables:
 * - $title: the (sanitized) title of the node.
 * - $content: Node body or teaser depending on $teaser flag.
 * - $picture: The authors picture of the node output from
 *   theme_user_picture().
 * - $date: Formatted creation date (use $created to reformat with
 *   format_date()).
 * - $links: Themed links like "Read more", "Add new comment", etc. output
 *   from theme_links().
 * - $name: Themed username of node author output from theme_user().
 * - $node_url: Direct url of the current node.
 * - $terms: the themed list of taxonomy term links output from theme_links().
 * - $submitted: themed submission information output from
 *   theme_node_submitted().
 *
 * Other variables:
 * - $node: Full node object. Contains data that may not be safe.
 * - $type: Node type, i.e. story, page, blog, etc.
 * - $comment_count: Number of comments attached to the node.
 * - $uid: User ID of the node author.
 * - $created: Time the node was published formatted in Unix timestamp.
 * - $zebra: Outputs either "even" or "odd". Useful for zebra striping in
 *   teaser listings.
 * - $id: Position of the node. Increments each time it's output.
 *
 * Node status variables:
 * - $teaser: Flag for the teaser state.
 * - $page: Flag for the full page state.
 * - $promote: Flag for front page promotion state.
 * - $sticky: Flags for sticky post setting.
 * - $status: Flag for published status.
 * - $comment: State of comment settings for the node.
 * - $readmore: Flags true if the teaser content of the node cannot hold the
 *   main body content.
 * - $is_front: Flags true when presented in the front page.
 * - $logged_in: Flags true when the current user is a logged-in member.
 * - $is_admin: Flags true when the current user is an administrator.
 *
 * @see template_preprocess()
 * @see template_preprocess_node()
 */
?>
<div id="resultscontainer">
	<div id="resultstop"></div>
	<div id="skeleton_cache">
	<?php
	$nid = $node->nid;
	$file = file_directory_path() . '/' . 'election' . '/' . strval($nid) . '-elec-skeleton' . '.php';
	if (file_exists($file) && is_file($file) && is_readable($file)){
	  include($file);
        }else{
          echo($file . ' file not found !');  
	};
	?>
	</div>
</div>

<?php if ($teaser) { ?>
<?php 
$string = cctv_functions_get_alias(arg(0), arg(1));
if ($string == 'groups'){ ?>
  <?php if ($page == 0) { ?>
  <div class="content"><a href="<?php print $node_url; ?>"><?php print $title?></a><span class="submitted"><?php print " ".$user_name. " " . $date; ?></span></div>
  <?php }; ?>
<?php }else{ ?>
<div class="node<?php if ($sticky) { print " sticky"; } ?><?php if (!$status) { print " node-unpublished"; } ?>">
  <?php if ($picture) {
      print $picture;
    }?>
  <?php if ($page == 0) { ?>
  <h2 class="title"><a href="<?php print $node_url; ?>"><?php print $title?></a></h2>
  <?php }; ?>
  <span class="submitted"><?php //print $submitted?></span> <span class="taxonomy"><?php //print $terms?></span>
  <div class="content"><?php print $content?></div>
  <div class="clr">
    <?php if ($links): ?>
      <div class="links"><?php //print $links; ?></div>
    <?php endif; ?>
  </div>
</div>
<?php } ?>
<?php 
} else { ?>


<div class="node <?php print $node_classes ?>" id="node-<?php print $node->nid; ?>"><div class="node-inner">

  <?php if ($page == 0): ?>
    <h2 class="title">
      <a href="<?php print $node_url; ?>"><?php print $title; ?></a>
    </h2>
  <?php endif; ?>

  <?php if ($unpublished) : ?>
    <div class="unpublished"><?php print t('Unpublished'); ?></div>
  <?php endif; ?>

  <?php if ($picture) //print $picture; ?>

  <?php if ($submitted): ?>
    <div class="submitted">
      <?php //print $submitted; ?>
    </div>
  <?php endif; ?>

  <?php if (count($taxonomy)): ?>
    <div class="taxonomy"><?php //print t(' in ') . $terms; ?></div>
  <?php endif; ?>

  <div class="content">
    <?php //print $content; ?>
  </div><!-- this is the drupal content for this drupal node-->

  <?php if ($links): ?>
    <div class="links">
      <?php //print $links; ?>
    </div>
  <?php endif; ?>

</div></div> <!-- /node-inner, /node -->

<?php } ?>
