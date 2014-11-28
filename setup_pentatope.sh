#!/bin/env bash
sudo yum install -y docker
sudo systemctl start docker.service
sudo docker run -t xanxys/pentatope ./pentatope --server
