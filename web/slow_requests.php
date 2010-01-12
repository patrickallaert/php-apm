<?php
$records = apm_get_slow_requests_count();
switch ($_GET['sidx']) {
    case 'time':
        $order = APM_ORDER_TIMESTAMP;
        break;
    case 'duration':
        $order = APM_ORDER_DURATION;
        break;
    case 'file':
        $order = APM_ORDER_FILE;
        break;
    case 'id':
    default:
        $order = APM_ORDER_ID;
}
?>
{
  total: "<?php echo ceil($records / $_GET['rows']) ?>",
  page: "<?php echo $_GET['page'] ?>",
  records: "<?php echo $records ?>",
  rows : [
    <?php
    apm_get_slow_requests($_GET['rows'], ($_GET['page'] - 1) * $_GET['rows'], $order, $_GET['sord'] === 'asc', true);
    ?>
  ]
}
