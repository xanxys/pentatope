"""
Provider protocol and its implementations.

A provider is an abstract entity that:

* runs pentatope intance (in docker) somewhere
* makes the instance available via HTTP RPC
* supports clean construction/deconstruction operation
* estimates time and money costs involved


As use of provider might invove real money, we need detailed invariance.
Namely, they should follow these:

* until .prepare is called, no cost should occur (external connection is ok)
* caller must .discard once they .prepare, even in rare events
  (such as SIGTERM)
** However, when .prepare throws ProviderValidationError,
   .discard should not be called, and the provider should clean
   any state before throwing the exception.

"""
from __future__ import print_function, division
import boto
import boto.ec2
import random
import subprocess
import time


class IncompatibleAPIError(Exception):
    """
    Raised when external API (such as AWS) seemed to have
    changed.
    """
    pass


class ProviderValidationError(Exception):
    """
    Raised when a Provider seems unusable despite
    availability of config data.
    (e.g. Invalid AWS credential format, dependency unsatisfied)
    """
    pass


class LocalProvider(object):
    """
    Provides local pentatope instances.
    LocalProvier assumes docker to be installed on the local system,
    and sudo works without password.
    """
    def __init__(self):
        if not self._is_docker_usable():
            raise ProviderValidationError("docker was not found")

    def _is_docker_usable(self):
        try:
            subprocess.check_output(["docker"])
            return True
        except OSError:
            return False

    def calc_bill(self):
        return [
            ("This machine", 0)]

    def prepare(self):
        self.image_name = "xanxys/pentatope-dev"
        self.container_name = "%s_%d" % (
            "pentatope_local_worker",
            random.randint(0, 1000))
        self.port = random.randint(35000, 50000)

        result = subprocess.check_output([
            "sudo", "docker", "run",
            "--detach=true",
            "--volume", "/home/xyx/repos/pentatope:/root/local",
            "--name", self.container_name,
            "--publish", "%d:80" % self.port,
            self.image_name,
            "/root/local/build/pentatope"])
        self.container_id = result.decode('utf-8').strip()
        time.sleep(1)  # wait server boot
        return ["http://localhost:%d/" % self.port]

    def discard(self):
        subprocess.call([
            "sudo", "docker", "rm", "-f", self.container_id])


class EC2Provider(object):
    """
    Provides remote pentatope instances on AWS EC2.
    """
    def __init__(self, credentials):
        """
        credentials must contain "access_key" and "secret_access_key".
        """
        self.conn = boto.ec2.connect_to_region(
            'us-west-1',
            aws_access_key_id=credentials["access_key"],
            aws_secret_access_key=credentials["secret_access_key"])
        if self.conn is None:
            raise ProviderValidationError("Couldn't connect to EC2")
        # Check whether credential is valid by performing a random
        # non-destructive action.
        try:
            self.conn.get_all_addresses()
        except boto.exception.EC2ResponseError as exc:
            raise ProviderValidationError(exc)

        self.price_per_hour = 0.232
        self.instance_type = 'c4.xlarge'

    def calc_bill(self):
        return [(
            "EC2 on-demand instance (%s) * 1" % self.instance_type,
            self.price_per_hour)]

    def prepare(self):
        pass

    def discard(self):
        pass
