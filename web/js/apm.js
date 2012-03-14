jQuery(document).ready(function(){
  var events = jQuery("#events");
  if(events.length > 0) {
	  events.jqGrid({
	    url: 'events.php',
	    datatype: 'json',
	    mtype: 'GET',
	    colNames :['#', 'Time', 'Type', 'File', 'Line', 'IP', 'Message'],
	    colModel :[
	      {name:'id', index:'id', width:55},
	      {name:'time', index:'time', width:130},
	      {name:'type', index:'type', width:70},
	      {name:'file', index:'file', width:300},
	      {name:'line', index:'line', width:50, align:'right', sortable:false},
	      {name:'ip', index:'ip', width:70},
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
	    gridComplete: function() {
	      var _rows = $(".jqgrow");
	      for (var i = 0; i < _rows.length; i++) {
	        _rows[i].attributes["class"].value += " " + _rows[i].childNodes[2].textContent;
	      }
	    },
	    onSelectRow: function(id) {
	      GB_show(jQuery("#events").jqGrid('getCell', id, 5),"event.php?id=" + jQuery("#events").jqGrid('getCell', id, 0), 90, 90);
	    },
	
	  });
  }
  
  var slowRequests =  jQuery("#slow-requests");
  if(slowRequests.length > 0) {
	  slowRequests.jqGrid({
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
	    caption: 'Slow requests'
	  });
  }
  
  var eventWindowSource = jQuery('#source');
  if(eventWindowSource.length > 0) {
	 var sourceToggle = jQuery('#source_toggle a');
	 sourceToggle.click(function() {
		 sourceToggle.text(eventWindowSource.is(':visible') ? 'Show source code' : 'Hide source code');
		 eventWindowSource.slideToggle();
		 
	 });
  }
  
});
