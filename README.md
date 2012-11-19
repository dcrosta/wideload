# Wideload - Load Testing with Hard Timing Constraints

Wideload was designed to meet the requirements of load testing a real-time
API, whose consumers impose a strict request time policy of 100ms, after
which they disconnect an HTTP keep-alive connection and reconnect. None of
the existing load generation tools could enforce the behavior of such
clients, so wideload was born.

Specifically, wideload is different from alternatives because it:

1. Uses HTTP keep-alive by default
2. Enforces a hard limit on request time, after which the attempt
   is marked as a failure, and the connection dropped
4. Can make GET or POST requests (with a fairly large request body, up to
   about 1MB)

And like many other alternatives in that it:

1. Allows configurable concurrency
2. Logs comprehensive request timing information for the full suite to CSV

However, wideload does not:

1. Infer any semantics from the order of URLs being tested, nor make any
   guarantees that they will be tested in the order listed (i.e. it is not
   a "functional" test tool)
2. Carry state from one request to the next (such as HTTP cookies)

# Usage

Widelaod is simple to use from the command-line:

    $ wideload --run-seconds 30 --concurrency 50 path/to/urls.txt

This will run 50 threads concurrently for 30 seconds each, where each thread
will request each of the URLs in the file `path/to/urls.txt` as many times
as it can until the time runs out.

The format of the URLs file is simple:

    GET<tab>http://my.server.com/url1
    GET<tab>http://my.server.com/url2
    POST<tab>http://my.server.com/url3<tab>bXkgcG9zdCBkYXRh

Each line is 2 or 3 tab-separated values:

1. The HTTP method (currently only GET and POST are supported)
2. The URL to request
3. For POST requests, base64-encoded POST body data.

Each line may be at most 1MB, which means that the maximum POST body size is
just under 1MB (depending on the length of the URL in question).

# License

Wideload is issued under the BSD license (see attached LICENSE file). It
uses several third party libraries which are distributed under their own
licenses, and includes [libb64](http://libb64.sourceforge.net) which is
placed in the public domain by its authors.
