# bitU the smart xmpp bot

This program is intended to be a simple, generic and extensible jabber
bot. One of the goals of the project was to provide a presence based
monitoring system. This is the reason for choosing xmpp.

After writing some lines of code, we saw that a generic solution could
be implemented. And then, we did it.

In the next few lines bellow you'll know how to install, use and patch
it.

## Installation

Before anything you have to install two basic libraries to be able to
see bitu working properly:
[taningia](https://github.com/clarete/taningia) and
[iksemel](http://code.google.com/p/iksemel/).

### iksemel

This library is available in various systems, including in the Debian
GNU/Linux. If you're using Debian, you can install this taningia
dependency by issuing the command:

    # aptitude install libiksemel-dev

If you are using Mac OS X, you can use
[this gist](https://gist.github.com/4012731) as
[homebrew](http://mxcl.github.com/homebrew/) formula

I'm currently using the 1.5 version of iksemel and things seem to work
properly.

### taningia

It is a generic comunication library that provides a simple xmpp client
lib (among other things).

To install it, you should first download it using the following
command:

    $ git clone https://github.com/clarete/taningia.git

After that, enter in the downloaded directory and run the `autogen.sh`
script, like this:

    $ cd taningia
    $ ./autogen.sh

The `autogen.sh` script will also call the `configure` script. So you
can pass the configure params directly to the `autogen.sh' one. Like
this:

    $ ./autogen.sh --prefix=/tmp/mytestbase

After that, you should compile the library. To do this, you can do the
following:

    $ make

But wait! If you're planning to use this library and never see a
bug... you should give up right now (just joking =D). But it is wise to
compile it with debug support when writting applications based on it. To
do it, execute the following command:

    $ make CFLAGS="-g -O0 -DDEBUG"

### bitu

Now it is time to install the bitu program itself. It is quite easy to
do this. Just follow the same stuff you did with taningia:

    $ ./autogen.sh && ./configure && make
    $ sudo make install


To get more information about the installation, please consulte the
`INSTALL` file.

## Basic usage

### Setup your environment

Since it is a jabber bot, you'll need an already configured jabber
server. All tests were done using `ejabberd' (also available in Debian).

First of all, go to the ejabberd administration and create two
users. For example, the `admin@localhost` and `server@localhost`.

Connect to the `admin@localhost` in your preferred xmpp client. And
then, add the `server@localhost` to your roster. Currently, the jabber
bot does not accept any presence subscription. You'll have to do it by
using the ejabberd web interface. Open a browser and point it to the
http://localhost:5280/admin address. Follow the way "Virtual Hosts" ->
"localhost" -> "Users" -> "server@localhost" -> "Roster" and then you'll
see the authorization request of the admin user.

### Loading plugins

We currently have two builtin plugins. The cpuinfo and the uptime
ones. Both are installed in $(libdir)/bitu. To use them, you will have
to load them to bitu. We have two ways for doing this. The first is to
create a config file and append something like this to the config file:

    load libprocessor.so
    load libuptime.so

Save the config file with any name anywhere. Now, you have to make sure
that the $(libdir)/bitu dir is inside LD_LIBRARY_PATH envvar. Do the
following to make sure:

    $ export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/path/to/your/lib/bitu

We're getting closer and closer to see it working, just follow the last
step and see what we can do =D

If you are using Mac OS, remember that `LD_LIBRARY_PATH` becomes
`DYLD_LIBRARY_PATH`.

### Launching bitu

Now, you just have to call bitu with the connection parameters and with
the plugin list file. Like this:

    $ bitu --jid=server@localhost \
           --password=<yourpasswd>  \
           --config-file=/path-to-your-config-file

If you receive any ConnectionError stuff, try to inform the host ip with
the parameter `--host`. like this:

    $ bitu --jid=server@localhost \
           --password=<yourpasswd>  \
           --host=127.0.0.1 \
           --plugins-config=/path-to-your-config-file

## Notes about configuration

Bitu is a xmpp bot so presence is an intrisec concept. To make it
available all the time, configuration must be changed at the runtime
with no need to restart anything.

There are some few configuration parameters that you will not be able to
change while bitU is running. Like the jid, password, host and local
server configuration. All other commands are possible to run using a
helper program called `bituctl`.

There are plans to listen to the `SIGHUP` and reload configuration file.
