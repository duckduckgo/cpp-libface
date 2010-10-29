= Quick Start Guide =

== Software required to run lib-face ===

  * [http://python.org/ Python (2.4-2.6)]
  * [http://mongrel2.org/home Mongrel2]
  * [http://zeromq.org/ ZeroMQ 2.0.8]
  * [http://www.sqlite.org/ SQlite3]
  * [http://www.zeromq.org/bindings:python ZeroMQ Python bindings]

=== Create the sqlite configuration file from mongrel2.conf ===

m2sh load -config mongrel2.conf

=== Start Mongrel2 ===

m2sh start -db config.sqlite -host localhost

=== Start lib-face ===

Go to the directory containing m2handler.py and type:
python m2handler.py

=== Input data file format ===

Your input data file should be a text file containing separate lines
for each phrase you want auto-completion on. Each line should be
formatted as:
FREQUENCY_COUNT\tPHRASE\n

For example:
10    hello world
15    How I met your mother
21    How do you do?
9     Great Days ahead

The importer will convert everything to lowercase. Your input text
file should be utf8 encoded.

=== Important URLs ===

You will mainly use 2 URLs:

1. The import URL: http://localhost:6767/face/import/?file=PATH_TO_INPUT_FILE

This will import everything in the file at PATH_TO_INPUT_FILE into
the currently running lib-face memory's instance.

2. The query URL: http://localhost:6767/face/suggest/?q=PREFIX&n=LIMIT

The n=LIMIT is optional and the maximum value you can supply is 16. If you
specify a number greater than 16, it will return only 16 results. This behaviour
can be changed by modifying the handler's code.

Author: Dhruv Matani
dhruvbird@gmail.com
