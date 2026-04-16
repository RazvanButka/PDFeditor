$(document).ready(function() {
    $('#soap-button').click(function() {
        // Construct SOAP request
        var soapRequest =
            `<?xml version="1.0" encoding="UTF-8"?>
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

        // Make SOAP request
        $.ajax({
            url: 'http://localhost:18082',
            type: 'POST',
            contentType: 'text/xml',
            data: soapRequest,
            success: function(data) {

                console.log(data);
                const xmlString = new XMLSerializer().serializeToString(data);

                console.log(xmlString);
                const parser = new DOMParser();
                const xmlDoc = parser.parseFromString(xmlString, "text/xml");

                const outElement = xmlDoc.getElementsByTagName("out")[0];
                const outValue = outElement.textContent;

                console.log(outValue);
                // Display result on page body
                $('#result').text(outValue);
            },
            error: function(xhr, status, error) {
                // Handle error
                console.log(error);
            }
        });
    });
});