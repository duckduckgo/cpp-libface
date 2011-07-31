"""
face: Fastest Auto-Complete in the East
An efficient auto-complete library suitable for large volumes of data.


Copyright (c) 2010, Dhruv Matani
dhruvbird@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
"""

from mongrel2 import handler
import re
import urllib
import cgi
from face import AutoComplete, Entry

try:
    import simplejson as json
except:
    import json

"""
This is the handler for mongrel2.

"""

# The ID of the sender handler
sender_id = "34f9ceee-cd52-4b7f-b197-88bf2f0ec378"

# Connections to ZMQ
conn = handler.Connection(sender_id, "tcp://127.0.0.1:9997",
                          "tcp://127.0.0.1:9996")

ac = AutoComplete()


def reply_404(conn, req):
    return conn.reply_http(req, '', 404, "Not Found")


def parse_querystring(qs):
    q = cgi.parse_qs(qs, True)
    for key in q:
        if len(q[key]) > 0:
            q[key] = q[key][0]
    return q



def handle_autocomplete(req, url, m):
    q = parse_querystring(req.headers.get('QUERY', ''))
    print "Query String: %s" % str(q)

    response = "[ ]"

    if "q" not in q:
        conn.reply_http(req, response)
        return

    phrase = q["q"].strip().encode('utf8').lower()
    if len(phrase) == 0:
        conn.reply_http(req, response)
        return

    n = 10
    try:
        if "n" in q:
            _n = int(q["n"])
            if _n <= 16 and _n > 0:
                n = _n
    except:
        pass

    print "Phrase: %s" % phrase

    options = ac.query(phrase, n)
    oa = [ ]
    for o in options:
        oa.append({"phrase": o.phrase.decode('utf8'), "score": o.score})
    response = json.dumps(oa)

    conn.reply_http(req, response)



def handle_import(req, url, m):
    from os import path

    if 1 == 0:
        ac.insert(Entry("how i met your mother", 10))
        ac.insert(Entry("how i got to know you", 5))
        ac.insert(Entry("how i found out about her", 25))
        ac.insert(Entry("where do you go?", 10))
        ac.insert(Entry("what have you been up to these days?", 80))
        ac.insert(Entry("how i made an aircraft", 11))
        ac.insert(Entry("another day another time", 6))
        ac.insert(Entry("dhruv matani", 6))
        ac.insert(Entry("duck duck go", 6))
        ac.insert(Entry("only for debugging", 6))
        ac.commit()
        conn.reply_http(req, 'SUCCESS')
        return

    q = parse_querystring(req.headers.get('QUERY', ''))
    print "Query String: %s" % str(q)

    file = ''
    if "file" in q:
        file = q["file"].strip()

    if not file or not path.exists(file):
        conn.reply_http(req, '')
        return

    f = open(file, "r")
    # print f
    l = f.readline()
    ctr = 0
    while l:
        # print l
        l = l.strip()
        c = l.split("\t")
        if len(c) != 2:
            print "%d: The line '%s' is not a TSV" % (ctr+1, l))

        e = Entry(c[1].lower(), int(c[0]))
        # print "Entry: " + str(e)
        ac.insert(e)
        ctr += 1
        l = f.readline()

    ac.commit()
    f.close()

    conn.reply_http(req, "SUCCESS: Added %d records" % ctr)
    return




handlers = [
    ( re.compile(r"/face/suggest/"), handle_autocomplete ),
    ( re.compile(r"/face/import/"), handle_import ),
]


def url_handler():
    import urlparse

    while True:
        req = conn.recv()
        print "URL: " + req.path

        # response = "<pre>" + str(dir(req)) + "</pre>"
        # print response
        # print req.path

        url = urlparse.urlparse(req.path)
        m = None

        for h in handlers:
            m = h[0].match(url.path)
            # print str(m.groups())
            if m:
                print "Handling request"
                h[1](req, url, m)
                break

        if not m:
            reply_404(conn, req)



if __name__ == "__main__":
    url_handler()
