# Hairgap

`hairgap` is a set of tools to transfer data over a unidirectional network link
(typically a network diode). It uses catid's `wirehair`
(https://github.com/catid/wirehair) librairy for error correction and is written
to handle high bandwith transfers (> 200 MB/s).

It is meant to be a mere transport on a dedicated, safe link: no authentication
nor encryption is guaranteed, although there is a work in progress on this
matter.

This should be considered alpha quality, any bug report will be very welcome.

## Usage

Before anything, to make high bandwith transfers work properly on a linux
machine, you might want to change at least these system options on the receiver
side:

```
net.core.rmem_max=67108864 # at least 4MB, 64MB is fine
net.core.rmem_default=67108864 # equal to net.core.rmem_max (if you dare)
net.core.netdev_max_backlog=10000 # works fine
```
and
```
net.ipv4.udp_rmem # multiply all three values by 32
```
Or, on newer kernels:
```
net.ipv4.udp_mem="49314528 65752736 3082158"
```


To use, on the receiver side first:

```sh
$ hairgapr LISTENING_IP > OUTPUT_FILE
```

Then, on the sender side:
```sh
$ hairgaps RECEIVER_IP < INPUT_FILE
```

see `hairgap[sr]` -h for various options. For very reliable transfers on
machines with a fast CPU, I would suggest `-N 30000 -r 1.5`, which sets a
relatively high redundancy (+50% of redundant data) and big redundancy blocks
for a better resistance to loss bursts (N=30000).

Note that a static ARP entry for `RECEIVER_IP` must be provided for `hairgaps`
to work properly. One way to achieve this is as follows:

```sh
# arp -s 10.0.0.1 aa:bb:cc:00:01:02
```

## Compiling

Compilation has only been tested on linux.

*Note:* you should either `git clone --recursive` or
`git submodules --init --update` to retrieve submodules.

```sh
$ make
```

Installing
----------

```sh
# make install
```

Testing
-------

Testing properly will require you to set the aforementioned `sysctl`s. Note
that the tests can currently deadlock, try to restart them. This is another
FIXME. More tests are on their way.

```sh
$ make test
```

Protocol
--------

Hairgap implements its own, very simple protocol. Its documentation is on its
way too.

Hacking
-------

See `hairgap.h` first, it will give you pointers.
