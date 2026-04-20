#!/bin/bash

# Create a simple SOAP request
SOAP_REQUEST="POST / HTTP/1.1
Host: localhost:18082
Content-Type: text/xml; charset=UTF-8
SOAPAction: \"http://www.example.org/operations/createFile\"
Content-Length: 500

<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:ns1=\"http://www.example.org/operations/\">
<SOAP-ENV:Body>
<ns1:createFile>
<in>test_file.txt</in>
</ns1:createFile>
</SOAP-ENV:Body>
</SOAP-ENV:Envelope>"

echo "Sending SOAP request to server..."
echo "$SOAP_REQUEST" | timeout 3 nc -q 1 localhost 18082
echo ""
echo "Done"
