<?php

namespace Drupal\yandex_turbo\Controller;

use Drupal\Core\Controller\ControllerBase;

/**
 * Returns responses for Yandex Turbo routes.
 */
class yandex_turbo extends ControllerBase {

  /**
   * Builds the response.
   */
  public function build() {

    $build['content'] = [
      '#type' => 'item',
      '#markup' => $this->t('It works!'),
    ];

    return $build;
  }

}
