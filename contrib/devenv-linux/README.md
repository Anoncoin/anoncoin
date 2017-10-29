# ANC Dev env


## Howto create devenv

In this directory run: `docker -t ancdevenv:latest .`


## Howto use it with ANC

In the git root directory, run `docker run --rm -ti -v $(pwd):/anoncoin ancdevenv:latest bash` and enjoy. ANC source is under /anoncoin.


