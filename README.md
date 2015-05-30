# Pentatope
Physically based renderer for 4-dimensional space.


This is *not* based on 4-dimensional version of Maxwell equation.
Rather, this is a natural extension of geometric optics
(and light transport equation) to 4-dimensional space.


Implementation details, especially class hierarchy is designed under
heavy influence of [pbrt](http://www.pbrt.org/).


## Project Structure
* `worker/`: C++ server code for worker node (runs in docker, either on AWS or on your local PC)
* `controller/`: Go client code for controller (runs on your PC natively)
* `docker/`: multiple dockerfiles for compiling(debugging), running, and common utils
* `example/`: generator of `RenderTask` or `RenderMovieTask` for various scenes
* `proto/`: serialization used everywhere (e.g. worker<->controller and example<->worker)

## Usage
Run `./build.sh`. It will create 3 docker images and run tests etc.
So, you'll need local docker installation for it. After build is done,
you usually don't need to run docker manually.

After that, you can run, for example, `./example/animation_demo.py` and `./controller/pentatope.py` to render it.

You need to supply your AWS credentials as a JSON file
(that contains "access_key" and "secret_access_key" as a top-level map),
if you are to use distributed rendering accessible via `--aws` flag in
`controller/pentatope.py`.


## Development
Although `./build.sh` will checkout latest commit (inside a container) and
build images based on it, it's too time-consuming for rapid iterations.

You can use `docker.io/xanxys/pentatope-dev` image and mounting local directory inside
a container. (e.g. `sudo docker run -t -i --rm -v $(pwd):/root/local -p 8080:80 docker.io/xanxys/pentatope-dev bash`)

## Credentials

### AWS (`--aws`)
Your access key in the following JSON:
```
{
	"access_key": "<Your Access Key>",
	"secret_access_key": "<Your Secret Access Key>"
}
```

### GCE (`--gce`)
Your service account oauth key in JSON format, as downloaded from the console.


## References
* [Mathematical proof etc. on google drive](https://docs.google.com/document/d/1lfWarQdW_cZsIxPnigJCLeeWBzgZ6UGsgGNOq_5b1J8/edit?usp=sharing) (commentable, non-editable)

* [Distributed render design doc on google drive](https://docs.google.com/document/d/1dSuWV-QI-f7r1uMlOeKNSTRCkkonnYRANqP-lg-rEHk/edit?usp=sharing)

* [pentatope on dockerhub](https://registry.hub.docker.com/u/xanxys/pentatope/)
