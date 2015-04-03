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
import urllib2


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
        self.image_name = "xanxys/pentatope-prod"
        self.container_name = "%s_%d" % (
            "pentatope_local_worker",
            random.randint(0, 1000))
        self.port = random.randint(35000, 50000)

        result = subprocess.check_output([
            "sudo", "docker", "run",
            "--detach=true",
            "--name", self.container_name,
            "--publish", "%d:80" % self.port,
            self.image_name,
            "/root/pentatope/pentatope"])
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

        self.price_per_hour = 1.856
        self.instance_type = 'c4.8xlarge'
        # self.price_per_hour = 0.232
        # self.instance_type = 'c4.xlarge'
        # self.price_per_hour = 0.116
        # self.instance_type = 'c4.large'
        self.sg_name = "pentatope_sg"

    def calc_bill(self):
        return [(
            "EC2 on-demand instance (%s) * 1" % self.instance_type,
            self.price_per_hour)]

    def prepare(self):
        self._setup_sg()

        boot_script = '\n'.join([
            '#cloud-config',
            'repo_upgrade: all',
            'packages:',
            ' - docker',
            'runcmd:',
            ' - ["/etc/init.d/docker", "start"]',
            ' - ["docker", "pull", "xanxys/pentatope-prod"]',
            ' - ["docker", "run", "--detach=true", "--publish", "8000:80", "xanxys/pentatope-prod", "/root/pentatope/pentatope"]'])
        image_id = 'ami-d114f295'  # us-west-1
        self.reservation = self.conn.run_instances(
            image_id,
            instance_type=self.instance_type,
            security_groups=[self.sg_name],
            user_data=boot_script)
        instance = self.reservation.instances[0]
        wait_limit = 100
        t_limit = time.time() + wait_limit
        # wait boot
        while time.time() < t_limit:
            if instance.update() == "running":
                break
            time.sleep(5)
        else:
            self.discard()
            raise ProviderValidationError(
                "Instance did not become running within %f sec" % wait_limit)
        url = "http://%s:8000/" % instance.ip_address
        print(url)
        # wait docker boot
        wait_limit = 250
        t_limit = time.time() + wait_limit
        while time.time() < t_limit:
            time.sleep(5)
            try:
                urllib2.urlopen(url, "PING", 1).read()
            except urllib2.HTTPError:
                return [url]  # expected response for a bogus request
            except urllib2.URLError as exc:
                print(exc)
                continue  # node not operating

        # Handle boot failure cleanly
        self.discard()
        raise ProviderValidationError(
            "Instance did not boot within %f sec limit" % wait_limit)

    def discard(self):
        self.conn.terminate_instances([self.reservation.instances[0].id])
        self._clean_sg()

    def _setup_sg(self):
        """
        Setup a security group that allows HTTP incoming packets
        in a idempotent way.
        """
        self._clean_sg()
        # Create
        self.sg = self.conn.create_security_group(
            self.sg_name,
            description="For workers in https://github.com/xanxys/pentatope")
        self.sg.authorize(
            ip_protocol="tcp",
            from_port="8000",
            to_port="8000",
            cidr_ip="0.0.0.0/0")

    def _clean_sg(self):
        """
        Remove the pentatope security group in a idempotent way.
        """
        while True:
            sgs = self.conn.get_all_security_groups()
            if all(sg.name != self.sg_name for sg in sgs):
                break  # no existing SG
            try:
                self.conn.delete_security_group(self.sg_name)
            except boto.exception.EC2ResponseError:
                pass  # dependency error is expected, because instance termination is slow
            finally:
                time.sleep(5)
