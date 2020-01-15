<?php

/**
 * Page callback: Generates an Yandex Turbo RSS
 *
 * @return string
 *   An XML formatted string.
 */
function yandex_turbo_page_rss($page = 0)
{
    $feeds      = array();
    $settings   = variable_get('yandex_turbo_node_types');
    $node_types = !empty($settings) ? array_keys(array_filter($settings)) : array();
    $orderBy    = variable_get('yandex_turbo_order_by', 'created');
    $order      = variable_get('yandex_turbo_order', 'DESC');
    $source     = variable_get('yandex_turbo_body', 'value');
	$on_page	= intval(variable_get('yandex_turbo_rss_max_item_page', '500'));
	$page		= intval($page) <= 0 ? 1 : intval($page);
	
	if ($on_page <= 0) {
		$on_page = 500;
	}
	
    if($node_types)
    {
        $query = db_select('node', 'n')
            ->fields('n', array('nid', 'title', 'created'))
            ->condition('status', NODE_PUBLISHED)
            ->condition('type', $node_types, 'IN');

        $query->leftJoin('field_data_body', 'd', '(d.entity_id = n.nid AND d.entity_type = :node)', array(
            ':node' => 'node'
        ));

        $query->addField('d', 'body_' . $source, 'body');

        $query->leftjoin('url_alias', 'ua', 'ua.source = CONCAT(:str, n.nid)', array(
            ':str'=>'node/'
        ));

        $query->fields('ua', array('alias'));

        if('Yes' == variable_get('yandex_turbo_sticky', 'No'))
        {
            $query->orderBy('n.sticky', 'DESC');
        }

        if('Yes' == variable_get('yandex_turbo_promoted', 'No'))
        {
            $query->orderBy('n.promote', 'DESC');
        }
		
		$itemStart = ($page * $on_page) - $on_page;
        $query->orderBy('n.' . $orderBy, $order)
            ->range($itemStart, $on_page);

        $query->where('d.body_' . $source . ' != :empty', array(':empty' => ''));

        $feeds = $query->execute()
            ->fetchAllAssoc('nid', PDO::FETCH_ASSOC);
    }
	
	if (count($feeds) <= 0) {
		drupal_not_found();
		return '';
	}
	
	return theme('yandex_turbo_page_rss', array('feeds' => $feeds));
}

/**
 * Prints the RSS page for a feed.
 *
 * @param $variables
 *   An associative array containing:
 *   - feeds: An array of the feeds to theme.
 *
 * @return string
 *
 * @ingroup themeable
 */
function theme_yandex_turbo_page_rss($variables)
{
    /**
     * Feed absolute link
     */
    $rss_link = url('yandex.turbo.rss', array('absolute' => TRUE));

    /**
     * Feeds data
     *
     * @var array
     */
    $feeds = $variables['feeds'];

    /**
     * Allowed HTMl tags
     * @see https://yandex.ru/support/webmaster/turbo/feed.html#rss-elements__item
     */
    $allowed_tags = array(
        'figure', 'h1', 'h2', 'p', 'br', 'ul', 'ol', 'li', 'b', 'strong', 'i', 'em', 'sup', 'sub',
        'ins', 'del', 'small', 'big', 'pre', 'abbr', 'u', 'a', 'img', 'figcaption', 'video', 'header'
    );

    /**
     * Removes CDATA tag from content
     *
     * @var string
     */
    $CDATA = '/^\s*\/\/<!\[CDATA\[([\s\S]*)\/\/\]\]>\s*\z/';

    /**
     * Items XML-string
     *
     * @var string
     */
    $items = '';

    /**
     * @vat string
     */
    $output = '';

    /**
     * Channel settings
     */
    $site_name           = variable_get('site_name', '');
    $site_slogan         = variable_get('site_slogan', 'Site description');
    $channel_title       = variable_get('yandex_turbo_rss_title', $site_name);
    $channel_description = variable_get('yandex_turbo_rss_description', $site_slogan);
    $channel_analytics   = variable_get('yandex_turbo_rss_analytics_type',   'Yandex');
    $max_item_images     = intval(variable_get('yandex_turbo_rss_max_item_images', '0'));
	
    if(!empty($feeds))
    {
        foreach ($feeds as $nid => $node)
        {
            $body = strip_tags(trim($node['body']), '<'.implode('><',$allowed_tags).'>');
            $body = preg_replace( $CDATA, '$1', $body );

            // Skip empty nodes
            if(!$body) continue;
			
			// Images limit
			if ($max_item_images > 0) {
				$find_images = 0;
				$body = preg_replace_callback ("/<img([^>]*)>/Us", function($matches) use (&$find_images, $max_item_images) {
					 $find_images++;
					 return ($find_images <= $max_item_images) ? '<img' . $matches[1] . '>' : '';
				}, $body);
			}
			
			// If the text is more than 400 characters, then Yandex requires you to arrange paragraphs, but we do from 350
			$content_text = strip_tags($body);
			if (strlen($body) > 350) {
				$body = preg_replace('~(?:\r?\n){2,}~',"\n",$body);
				$body = explode("\n", $body);
				foreach ($body as $line_key => $line_val) {
					if (!preg_match("/(\<p|\<br|\<ol|\<ul|\<li)/is", $line_val)) {
						$body[ $line_key ] .= '<br>';
					}
				}
				$body = implode("\n", $body);
			}
			
			// Make a headline
			$body = "<header><h1>".check_plain($node['title'])."</h1></header>" .$body;

			// Link to node
            $node_uri = $node['alias'] ? $node['alias'] : 'node/' . $nid;

            $data = array(
                array(
                    'key' => 'title',
                    'value' => check_plain($node['title']),
                ),
                array(
                    'key' => 'link',
                    'value' => url( $node_uri, array('absolute' => TRUE) ),
                ),

                array(
                    'key' => 'pubDate',
                    'value' => date(DateTime::RFC822, $node['created']),
                ),
                array(
                    'key' => 'turbo:content',
                    'value' => '<![CDATA[' . PHP_EOL . $body . PHP_EOL . ']]>',
                    'encoded' => TRUE,
                )
            );

            $items .= format_xml_elements(array(
                array(
                    'key'     => 'item',
                    'value'   => PHP_EOL . format_xml_elements($data),
                    'encoded' => TRUE,
                    'attributes' => array(
                        'turbo' => 'true',
                    )
                )
            ));
        }

        $body = $node = $node_uri = $header = $h1 = null;
    }

    drupal_add_http_header('Content-Type', 'application/rss+xml; charset=utf-8');
    
	$cms_plugin = format_xml_elements(array(
		array(
			'key' => 'turbo:cms_plugin',
			'value' => 'C2364A7B7F4160D5B2DC1718E3AE4970'
		)
	));
	$items = $cms_plugin . $items;

    $rss = array(
        'key' => 'rss',
        'value' => PHP_EOL . format_rss_channel( $channel_title, $rss_link, $channel_description, $items ),
        'encoded' => TRUE,
        'attributes' => array(
            'xmlns:yandex' => 'http://news.yandex.ru',
            'xmlns:media' => 'http://search.yahoo.com/mrss/',
            'xmlns:turbo' => 'http://search.yahoo.com/mrss/',
            'version' => '2.0',
        ),
    );

    print "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        . PHP_EOL
        . format_xml_elements( array( $rss ) );
}