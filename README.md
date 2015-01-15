**This project is incomplete and very far from ready for production
networks, even small ones.** There are gobs of critical bugs and missing
features that will be cleared up for the 0.1 release, but until then,
this software should not be used for anything but testing the software.

# ircd-micro 0.1-alpha1

A next-generation replacement for charybdis.


## Why?

Because it's fun, and I want to finish an IRCD some day just to say I did.


## Support

There is no official ircd-micro support, but you can make progress by
bugging irc.interlinked.me #code


## Building

If this source has been obtained via git, the following command should
be run first:

    $ git submodule update --init

You will need to run `git submodule update` for each successive pull.
ircd-micro can be then compiled and installed using the typical sequence:

    $ ./configure
    $ make
    $ make install

By default, ircd-micro will be installed into `~/ircd`, but this can be
changed with the `--prefix` argument to `./configure`.


## Running

Invoke micro with `-h` to get a list of options. By default,
configuration goes in `etc/micro.conf` relative to the current
directiory. An example configuration file can be found in
`doc/micro.conf.example`

# Credits

The following people have contributed to `ircd-micro` in various large
capacities over the project's lifetime, and the product would not be the same
without their additions:

- Hugo Landau (``sam` ``) -- Added in-flight upgrade system (`/upgrade`)
- Aaron Jones (`Aaron`) -- Added IPv6 support and operator CHALLENGE
  functionality.
- William Pitcock (`kaniini`) -- Redid configuration system to use the
  `libmowgli-2` configuration parser.
- Elizabeth Myers (`Elizafox`) -- Added help system, rate limiting, and a few
  client protocol commands.

In addition, the following people have made contributions as well:

- Andrew Wilcox (`awilcox`)
- Rylee Fowler (`Rylee`)
- Lee Starnes (`lstarnes`)
- Jessica Williams (`Quora`)
- Sam Dodrill (`Xe`)
- ShadowNinja
- ente
