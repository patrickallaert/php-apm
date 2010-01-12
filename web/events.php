<?php
$records = apm_get_events_count();
switch ($_GET['sidx']) {
    case 'time':
        $order = APM_ORDER_TIMESTAMP;
        break;
    case 'type':
        $order = APM_ORDER_TYPE;
        break;
    case 'file':
        $order = APM_ORDER_FILE;
        break;
    case 'line':
    case 'msg':
    case 'trace':
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
    apm_get_events($_GET['rows'], ($_GET['page'] - 1) * $_GET['rows'], $order, $_GET['sord'] === 'asc', true);
    ?>
  ]
}
