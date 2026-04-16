function selecteazaopt(){
   $(document).ready(function(ee){
      // ee.preventDefault();
       var ajaxdata = '';
       var opt = parseInt($('#opt').val());
       console.log(opt);
       if(opt == 2) {
        ajaxdata=`<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:ns1="http://www.example.org/operations/">
 <SOAP-ENV:Body>
   <ns1:clientLogFiles>
    <in></in>
   </ns1:clientLogFiles>
 </SOAP-ENV:Body>
</SOAP-ENV:Envelope>`;
       }
       else if(opt == 3){
           ajaxdata=`<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:ns1="http://www.example.org/operations/">
 <SOAP-ENV:Body>
   <ns1:createFile>
    <in>` + $('#creat_file').val() + `</in>
   </ns1:createFile>
 </SOAP-ENV:Body>
</SOAP-ENV:Envelope>
`;
       }
       else if(opt == 4){
           ajaxdata=`<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:ns1="http://www.example.org/operations/">
 <SOAP-ENV:Body>
   <ns1:filesInCertainProgLanguage>
    <in>` + $('#prog_lang_opt').val() + `</in>
   </ns1:filesInCertainProgLanguage>
 </SOAP-ENV:Body>
</SOAP-ENV:Envelope>
`;
       }
       else if(opt == 5){
           ajaxdata=`<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:ns1="http://www.example.org/operations/">
 <SOAP-ENV:Body>
   <ns1:filesStatistics>
    <in></in>
   </ns1:filesStatistics>
 </SOAP-ENV:Body>
</SOAP-ENV:Envelope>
`;
       }
    else if(opt == 6){
        ajaxdata=`<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:ns1="http://www.example.org/operations/">
 <SOAP-ENV:Body>
   <ns1:largestServFile>
    <in></in>
   </ns1:largestServFile>
 </SOAP-ENV:Body>
</SOAP-ENV:Envelope>
`;
       }
    else if(opt == 7){
        ajaxdata=`<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:ns1="http://www.example.org/operations/">
 <SOAP-ENV:Body>
   <ns1:modifyFile>
    <in>`+ $('#fname').val() + ' ' + $('#newfname').val() + `</in>
   </ns1:modifyFile>
 </SOAP-ENV:Body>
</SOAP-ENV:Envelope>`
;
       }
    else if(opt == 8){
        ajaxdata = `<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:ns1="http://www.example.org/operations/">
 <SOAP-ENV:Body>
   <ns1:removeFile>
    <in>` + $('#delfile').val() + `</in>
   </ns1:removeFile>
 </SOAP-ENV:Body>
</SOAP-ENV:Envelope>
`;
       }
  else if(opt == 9){
      ajaxdata=  `<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:ns1="http://www.example.org/operations/">
 <SOAP-ENV:Body>
   <ns1:servFiles>
    <in></in>
   </ns1:servFiles>
 </SOAP-ENV:Body>
</SOAP-ENV:Envelope>
`;
       }
  else if(opt == 10){
      ajaxdata = `<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:ns1="http://www.example.org/operations/">
 <SOAP-ENV:Body>
   <ns1:smallestServFile>
    <in></in>
   </ns1:smallestServFile>
 </SOAP-ENV:Body>
</SOAP-ENV:Envelope>
`;
       }
  else ajaxdata = `<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:ns1="http://www.example.org/operations/">
 <SOAP-ENV:Body>
   <ns1:totalServFilesSize>
    <in></in>
   </ns1:totalServFilesSize>
 </SOAP-ENV:Body>
</SOAP-ENV:Envelope>
`;
  console.log(ajaxdata);
       $.ajax({
           type: "POST",
           url: "http://localhost:18082",
           datatype: "XML",
           contentType: "text/xml",
           data: ajaxdata,
           success: function(rsp){
               console.log('Success!');
               var final = new XMLSerializer().serializeToString(rsp);
               console.log(final);
               $('body').append(`<p>` + final.toString() + `</p>`);
           },
           error: function(err, xhr, rsp){
               console.log('ERROR!');
               console.log(err + xhr + rsp);
           //    console.log(new XMLSerializer().serializeToString(err) + xhr + rsp);
           },
           }
       );
   });
}