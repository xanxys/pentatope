# Pentatope
Physically based renderer for 4-dimensional space.


This is *not* based on 4-dimensional version of Maxwell equation.
Rather, this is a natural extension of geometric optics
(and light transport equation) to 4-dimensional space.


Implementation details, especially class hierarchy is designed under
heavy influence of [pbrt](http://www.pbrt.org/).


## Project Structure
* `src/`: contains C++ code for worker node (runs on AWS)
* `controller/`: contains python code for controller (runs on your PC)

## Content Viewer
[WIP](https://xanxys.github.io/pentatope)

## References
* [Mathematical proof etc. on google drive](https://docs.google.com/document/d/1lfWarQdW_cZsIxPnigJCLeeWBzgZ6UGsgGNOq_5b1J8/edit?usp=sharing) (commentable, non-editable)

* [Distributed render design doc on google drive](https://docs.google.com/document/d/1dSuWV-QI-f7r1uMlOeKNSTRCkkonnYRANqP-lg-rEHk/edit?usp=sharing)

* [pentatope on dockerhub](https://registry.hub.docker.com/u/xanxys/pentatope/)
