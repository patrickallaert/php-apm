jQuery(document).ready(function(){
    jQuery("#events").jqGrid({
        url: 'events.php',
        datatype: 'json',
        mtype: 'GET',
        colNames :['#', 'Time', 'Type', 'URL', 'File', 'Line', 'IP', 'Message'],
        colModel :[
            {name:'id', index:'id', width:35},
            {name:'time', index:'time', width:100},
            {name:'type', index:'type', width:60},
            {name:'url', index:'url', width:150, sortable:true},
            {name:'file', index:'file', width:300},
            {name:'line', index:'line', width:30, align:'right', sortable:false},
            {name:'ip', index:'ip', width:50},
            {name:'msg', index:'msg', width:250, sortable:false}
        ],
        pager: '#events-pager',
        rowNum: 20,
        rowList: [10,20,50,100],
        sortname: 'id',
        sortorder: 'desc',
        viewrecords: true,
        hoverrows: false,
        autowidth: true,
        height: 'auto',
        caption: 'Events',
        hidegrid: false,
        gridComplete: function() {
            var _rows = $(".jqgrow");
            for (var i = 0; i < _rows.length; i++) {
                _rows[i].attributes["class"].value += " " + _rows[i].childNodes[2].textContent;
            }
        },
        onSelectRow: function(id) {
            document.location.href = 'event.php?id=' + id;
        },

      });
  
    jQuery("#slow-requests").jqGrid({
        url: 'slow_requests.php',
        datatype: 'json',
        mtype: 'GET',
        colNames: ['#', 'Time', 'Duration', 'File'],
        colModel :[
            {name:'id', index:'id', width:55},
            {name:'time', index:'time', width:130},
            {name:'duration', index:'duration', width:70},
            {name:'file', index:'file', width:300},
        ],
        pager: '#slow-requests-pager',
        rowNum: 20,
        rowList: [10,20,50,100],
        sortname: 'id',
        sortorder: 'desc',
        viewrecords: true,
        hoverrows: true,
        autowidth: true,
        height: 'auto',
        caption: 'Slow requests',
        hidegrid: false
    });
  
    var eventWindowSource = jQuery('#source');
    if (eventWindowSource.length > 0) {
        var sourceToggle = jQuery('#source_toggle a');
        sourceToggle.click(function() {
            sourceToggle.text(eventWindowSource.is(':visible') ? 'Show source code' : 'Hide source code');
            eventWindowSource.slideToggle();
        });
    }
});
