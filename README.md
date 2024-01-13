# sst-forza-elements

This repo contains the following elements:
- ZEN
- ZIP
- ZQM

Note that that the ZAP and RZA are implemented as part of [forzarev](https://github.com/tactcomplabs/forzarev)

## How to use this repo

1) Clone it on your development machine of choice.
2) Create a branch or checkout the branch for your element
3) Create sub-directories for your component under `src`, `include`, and `test`.
4) Create files to implement your element.
5) Write small SST tests to test your element.
   * Follow the example at https://github.com/tactcomplabs/forzarev/tree/devel/test/FORZA/forza_send for CMake tests
   * Any test (Makefile or CMake) is better than no tests! Please just document with your component how to compiler and run your test

## Info on using with ForzaRev

Taken from the [ForzaRev README](https://github.com/tactcomplabs/forzarev/blob/devel/README-FORZA.md).

```
git clone git@github.com:tactcomplabs/forzarev.git
cd forzarev
git checkout devel
git clone https://github.gatech.edu/FORZA/sst-forza-elements.git forza-elts
cd forza-elts
git checkout <branch_name; devel is the stable branch>
```
