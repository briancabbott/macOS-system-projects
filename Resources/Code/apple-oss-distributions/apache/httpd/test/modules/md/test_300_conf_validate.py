# test mod_md basic configurations

import re
import time
from datetime import datetime, timedelta

import pytest

from .md_conf import MDConf
from .md_env import MDTestEnv


@pytest.mark.skipif(condition=not MDTestEnv.has_acme_server(),
                    reason="no ACME test server configured")
class TestConf:

    @pytest.fixture(autouse=True, scope='class')
    def _class_scope(self, env):
        env.clear_store()

    # test case: just one MDomain definition
    def test_md_300_001(self, env):
        MDConf(env, text="""
            MDomain not-forbidden.org www.not-forbidden.org mail.not-forbidden.org
            """).install()
        assert env.apache_restart() == 0

    # test case: two MDomain definitions, non-overlapping
    def test_md_300_002(self, env):
        MDConf(env, text="""
            MDomain not-forbidden.org www.not-forbidden.org mail.not-forbidden.org
            MDomain example2.org www.example2.org mail.example2.org
            """).install()
        assert env.apache_restart() == 0

    # test case: two MDomain definitions, exactly the same
    def test_md_300_003(self, env):
        assert env.apache_stop() == 0
        MDConf(env, text="""
            MDomain not-forbidden.org www.not-forbidden.org mail.not-forbidden.org test3.not-forbidden.org
            MDomain not-forbidden.org www.not-forbidden.org mail.not-forbidden.org test3.not-forbidden.org
            """).install()
        assert env.apache_fail() == 0

    # test case: two MDomain definitions, overlapping
    def test_md_300_004(self, env):
        assert env.apache_stop() == 0
        MDConf(env, text="""
            MDomain not-forbidden.org www.not-forbidden.org mail.not-forbidden.org test3.not-forbidden.org
            MDomain example2.org test3.not-forbidden.org www.example2.org mail.example2.org
            """).install()
        assert env.apache_fail() == 0

    # test case: two MDomains, one inside a virtual host
    def test_md_300_005(self, env):
        MDConf(env, text="""
            MDomain not-forbidden.org www.not-forbidden.org mail.not-forbidden.org test3.not-forbidden.org
            <VirtualHost *:12346>
                MDomain example2.org www.example2.org www.example3.org
            </VirtualHost>
            """).install()
        assert env.apache_restart() == 0

    # test case: two MDomains, one correct vhost name
    def test_md_300_006(self, env):
        MDConf(env, text="""
            MDomain not-forbidden.org www.not-forbidden.org mail.not-forbidden.org test3.not-forbidden.org
            <VirtualHost *:12346>
                ServerName example2.org
                MDomain example2.org www.example2.org www.example3.org
            </VirtualHost>
            """).install()
        assert env.apache_restart() == 0

    # test case: two MDomains, two correct vhost names
    def test_md_300_007(self, env):
        MDConf(env, text="""
            MDomain not-forbidden.org www.not-forbidden.org mail.not-forbidden.org test3.not-forbidden.org
            <VirtualHost *:12346>
                ServerName example2.org
                MDomain example2.org www.example2.org www.example3.org
            </VirtualHost>
            <VirtualHost *:12346>
                ServerName www.example2.org
            </VirtualHost>
            """).install()
        assert env.apache_restart() == 0

    # test case: two MDomains, overlapping vhosts
    def test_md_300_008(self, env):
        MDConf(env, text="""
            MDomain not-forbidden.org www.not-forbidden.org mail.not-forbidden.org test3.not-forbidden.org
            <VirtualHost *:12346>
                ServerName example2.org
                ServerAlias www.example3.org
                MDomain example2.org www.example2.org www.example3.org
            </VirtualHost>

            <VirtualHost *:12346>
                ServerName www.example2.org
                ServerAlias example2.org
            </VirtualHost>
            """).install()
        assert env.apache_restart() == 0

    # test case: vhosts with overlapping MDs
    def test_md_300_009(self, env):
        assert env.apache_stop() == 0
        conf = MDConf(env)
        conf.add("""
            MDMembers manual
            MDomain not-forbidden.org www.not-forbidden.org mail.not-forbidden.org test3.not-forbidden.org
            MDomain example2.org www.example2.org www.example3.org
            """)
        conf.add_vhost(port=12346, domains=["example2.org", "www.example3.org"], with_ssl=True)
        conf.add_vhost(port=12346, domains=["www.example2.org", "example2.org"], with_ssl=True)
        conf.add_vhost(port=12346, domains=["not-forbidden.org", "example2.org"], with_ssl=True)
        conf.install()
        assert env.apache_fail() == 0
        env.apache_stop()
        env.httpd_error_log.ignore_recent()

    # test case: MDomain, vhost with matching ServerAlias
    def test_md_300_010(self, env):
        conf = MDConf(env)
        conf.add("""
            MDomain not-forbidden.org www.not-forbidden.org mail.not-forbidden.org test3.not-forbidden.org

            <VirtualHost *:12346>
                ServerName not-forbidden.org
                ServerAlias test3.not-forbidden.org
            </VirtualHost>
            """)
        conf.install()
        assert env.apache_restart() == 0

    # test case: MDomain, misses one ServerAlias
    def test_md_300_011a(self, env):
        env.apache_stop()
        conf = MDConf(env, text="""
            MDomain not-forbidden.org manual www.not-forbidden.org mail.not-forbidden.org test3.not-forbidden.org
        """)
        conf.add_vhost(port=env.https_port, domains=[
            "not-forbidden.org", "test3.not-forbidden.org", "test4.not-forbidden.org"
        ])
        conf.install()
        assert env.apache_fail() == 0
        env.apache_stop()

    # test case: MDomain, misses one ServerAlias, but auto add enabled
    def test_md_300_011b(self, env):
        env.apache_stop()
        MDConf(env, text="""
            MDomain not-forbidden.org auto mail.not-forbidden.org

            <VirtualHost *:%s>
                ServerName not-forbidden.org
                ServerAlias test3.not-forbidden.org
                ServerAlias test4.not-forbidden.org
            </VirtualHost>
            """ % env.https_port).install()
        assert env.apache_restart() == 0

    # test case: MDomain does not match any vhost
    def test_md_300_012(self, env):
        MDConf(env, text="""
            MDomain example012.org www.example012.org
            <VirtualHost *:12346>
                ServerName not-forbidden.org
                ServerAlias test3.not-forbidden.org
            </VirtualHost>
            """).install()
        assert env.apache_restart() == 0

    # test case: one md covers two vhosts
    def test_md_300_013(self, env):
        MDConf(env, text="""
            MDomain example2.org test-a.example2.org test-b.example2.org
            <VirtualHost *:12346>
                ServerName test-a.example2.org
            </VirtualHost>
            <VirtualHost *:12346>
                ServerName test-b.example2.org
            </VirtualHost>
            """).install()
        assert env.apache_restart() == 0

    # test case: global server name as managed domain name
    def test_md_300_014(self, env):
        MDConf(env, text=f"""
            MDomain www.{env.http_tld} www.example2.org

            <VirtualHost *:12346>
                ServerName www.example2.org
            </VirtualHost>
            """).install()
        assert env.apache_restart() == 0

    # test case: valid pkey specification
    def test_md_300_015(self, env):
        MDConf(env, text="""
            MDPrivateKeys Default
            MDPrivateKeys RSA
            MDPrivateKeys RSA 2048
            MDPrivateKeys RSA 3072
            MDPrivateKeys RSA 4096
            """).install()
        assert env.apache_restart() == 0

    # test case: invalid pkey specification
    @pytest.mark.parametrize("line,exp_err_msg", [
        ("MDPrivateKeys", "needs to specify the private key type"), 
        ("MDPrivateKeys Default RSA 1024", "'Default' allows no other parameter"),
        ("MDPrivateKeys RSA 1024", "must be 2048 or higher"),
        ("MDPrivateKeys RSA 1024", "must be 2048 or higher"),
        ("MDPrivateKeys rsa 2048 rsa 4096", "two keys of type 'RSA' are not possible"),
        ("MDPrivateKeys p-256 secp384r1 P-256", "two keys of type 'P-256' are not possible"),
        ])
    def test_md_300_016(self, env, line, exp_err_msg):
        MDConf(env, text=line).install()
        assert env.apache_fail() == 0
        assert exp_err_msg in env.apachectl_stderr

    # test case: invalid renew window directive
    @pytest.mark.parametrize("line,exp_err_msg", [
        ("MDRenewWindow dec-31", "has unrecognized format"), 
        ("MDRenewWindow 1y", "has unrecognized format"), 
        ("MDRenewWindow 10 d", "takes one argument"), 
        ("MDRenewWindow 102%", "a length of 100% or more is not allowed.")])
    def test_md_300_017(self, env, line, exp_err_msg):
        MDConf(env, text=line).install()
        assert env.apache_fail() == 0
        assert exp_err_msg in env.apachectl_stderr

    # test case: invalid uri for MDProxyPass
    @pytest.mark.parametrize("line,exp_err_msg", [
        ("MDHttpProxy", "takes one argument"), 
        ("MDHttpProxy localhost:8080", "scheme must be http or https"),
        ("MDHttpProxy https://127.0.0.1:-443", "invalid port"),
        ("MDHttpProxy HTTP localhost 8080", "takes one argument")])
    def test_md_300_018(self, env, line, exp_err_msg):
        MDConf(env, text=line).install()
        assert env.apache_fail() == 0, "Server accepted test config {}".format(line)
        assert exp_err_msg in env.apachectl_stderr

    # test case: invalid parameter for MDRequireHttps
    @pytest.mark.parametrize("line,exp_err_msg", [
        ("MDRequireHTTPS yes", "supported parameter values are 'temporary' and 'permanent'"),
        ("MDRequireHTTPS", "takes one argument")])
    def test_md_300_019(self, env, line, exp_err_msg):
        MDConf(env, text=line).install()
        assert env.apache_fail() == 0, "Server accepted test config {}".format(line)
        assert exp_err_msg in env.apachectl_stderr

    # test case: invalid parameter for MDMustStaple
    @pytest.mark.parametrize("line,exp_err_msg", [
        ("MDMustStaple", "takes one argument"), 
        ("MDMustStaple yes", "supported parameter values are 'on' and 'off'"),
        ("MDMustStaple true", "supported parameter values are 'on' and 'off'")])
    def test_md_300_020(self, env, line, exp_err_msg):
        MDConf(env, text=line).install()
        assert env.apache_fail() == 0, "Server accepted test config {}".format(line)
        assert exp_err_msg in env.apachectl_stderr
        env.httpd_error_log.ignore_recent()

    # test case: alt-names incomplete detection, github isse #68
    def test_md_300_021(self, env):
        env.apache_stop()
        conf = MDConf(env, text="""
            MDMembers manual
            MDomain secret.com
            """)
        conf.add_vhost(port=12344, domains=[
            "not.secret.com", "secret.com"
        ])
        conf.install()
        assert env.apache_fail() == 0
        # this is unreliable on debian
        #assert env.httpd_error_log.scan_recent(
        #    re.compile(r'.*Virtual Host not.secret.com:0 matches Managed Domain \'secret.com\', '
        #               'but the name/alias not.secret.com itself is not managed. A requested '
        #               'MD certificate will not match ServerName.*'), timeout=10
        #)

    # test case: use MDRequireHttps in an <if> construct, but not in <Directory
    def test_md_300_022(self, env):
        MDConf(env, text="""
            MDomain secret.com
            <If "1 == 1">
              MDRequireHttps temporary
            </If>
            <VirtualHost *:12344>
                ServerName secret.com
            </VirtualHost>
            """).install()
        assert env.apache_restart() == 0

    # test case: use MDRequireHttps not in <Directory
    def test_md_300_023(self, env):
        conf = MDConf(env, text="""
            MDomain secret.com
            <Directory /tmp>
              MDRequireHttps temporary
            </Directory>
            """)
        conf.add_vhost(port=12344, domains=["secret.com"])
        conf.install()
        assert env.apache_fail() == 0

    # test case: invalid parameter for MDCertificateAuthority
    @pytest.mark.parametrize("ca,exp_err_msg", [
        ("", "takes one argument"),
        ("yes", "The CA name 'yes' is not known "),
    ])
    def test_md_300_024(self, env, ca, exp_err_msg):
        conf = MDConf(env, text=f"""
            MDCertificateAuthority {ca}
            MDRenewMode manual  # lets not contact these in testing
        """)
        conf.install()
        assert env.apache_fail() == 0
        assert exp_err_msg in env.apachectl_stderr

    # test case: valid parameter for MDCertificateAuthority
    @pytest.mark.parametrize("ca, url", [
        ("LetsEncrypt", "https://acme-v02.api.letsencrypt.org/directory"),
        ("letsencrypt", "https://acme-v02.api.letsencrypt.org/directory"),
        ("letsencrypt-test", "https://acme-staging-v02.api.letsencrypt.org/directory"),
        ("LETSEncrypt-TESt", "https://acme-staging-v02.api.letsencrypt.org/directory"),
        ("buypass", "https://api.buypass.com/acme/directory"),
        ("buypass-test", "https://api.test4.buypass.no/acme/directory"),
    ])
    def test_md_300_025(self, env, ca, url):
        domain = f"test1.{env.http_tld}"
        conf = MDConf(env, text=f"""
            MDCertificateAuthority {ca}
            MDRenewMode manual
        """)
        conf.add_md([domain])
        conf.install()
        assert env.apache_restart() == 0, "Server did not accepted CA '{}'".format(ca)
        md = env.get_md_status(domain)
        assert md['ca']['url'] == url

