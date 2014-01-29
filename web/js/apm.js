jQuery(document).ready(function(){
    jQuery("#events").jqGrid({
        url: 'requests.php',
        datatype: 'json',
        mtype: 'GET',
        colNames :['#', 'Time', 'Host', 'URL', 'Script', '# events'],
        colModel :[
            {name:'id', index:'id', width:40},
            {name:'timestamp', index:'timestamp', width:130},
            {name:'host', index:'host', width:120},
            {name:'uri', index:'uri', width:400, sortable:true},
            {name:'script', index:'script', width:400},
            {name:'eventsCount', index:'eventsCount', width:60}
        ],
        pager: '#events-pager',
        rowNum: 30,
        rowList: [10,20,30,50,100],
        sortname: 'id',
        sortorder: 'desc',
        viewrecords: true,
        hoverrows: false,
        autowidth: false,
        height: 'auto',
        caption: 'Overview',
        hidegrid: false,
        gridComplete: function() {
            var _rows = $(".jqgrow");
            for (var i = 0; i < _rows.length; i++) {
                _rows[i].attributes["class"].value += " " + _rows[i].childNodes[2].textContent;
            }
        },
        onSelectRow: function(id) {
            document.location.href = 'request.php?id=' + id;
        },

      });

    jQuery("#stats").jqGrid({
        url: 'requests.php?stats',
        datatype: 'json',
        mtype: 'GET',
        colNames :['#', 'Time', 'Host', 'URL', 'Script', 'Duration', 'User CPU', 'Sys CPU'],
        colModel :[
            {name:'id', index:'id', width:40},
            {name:'timestamp', index:'timestamp', width:130},
            {name:'host', index:'host', width:120},
            {name:'uri', index:'uri', width:400, sortable:true},
            {name:'script', index:'script', width:400},
            {name:'duration', index:'duration', width:50},
            {name:'user_cpu', index:'user_cpu', width:50},
            {name:'sys_cpu', index:'sys_cpu', width:50}
        ],
        pager: '#stats-pager',
        rowNum: 30,
        rowList: [10,20,30,50,100],
        sortname: 'id',
        sortorder: 'desc',
        viewrecords: true,
        hoverrows: false,
        autowidth: false,
        height: 'auto',
        caption: 'Overview',
        hidegrid: false,
        gridComplete: function() {
            var _rows = $(".jqgrow");
            for (var i = 0; i < _rows.length; i++) {
                _rows[i].attributes["class"].value += " " + _rows[i].childNodes[2].textContent;
            }
        },
        onSelectRow: function(id) {
            document.location.href = 'request.php?id=' + id;
        },

      });

    jQuery("#request-details").jqGrid({
        url: 'events.php?id=' + decodeURIComponent((new RegExp('[?]id=([0-9]+)').exec(location.search))[1]),
        datatype: 'json',
        mtype: 'GET',
        colNames :['#', 'Time', 'Type', 'Message', 'File', 'Line'],
        colModel :[
            {name:'id', index:'id', width:40},
            {name:'timestamp', index:'timestamp', width:130},
            {name:'type', index:'type', width:80},
            {name:'message', index:'message', width:400},
            {name:'file', index:'file', width:400},
            {name:'line', index:'line', width:40, sortable:false}
        ],
        pager: '#stats-pager',
        rowNum: 30,
        rowList: [10,20,30,50,100],
        sortname: 'id',
        sortorder: 'asc',
        viewrecords: true,
        hoverrows: false,
        autowidth: false,
        height: 'auto',
        caption: 'Overview',
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
});
