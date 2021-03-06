<?php
/*
 * acme.inc
 * 
 * part of pfSense (https://www.pfsense.org/)
 * Copyright (c) 2016 PiBa-NL
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

namespace { // global code
	function acme_plugin_certificates($pluginparams) {
		global $config;
		$result = array();
		if ($pluginparams['type'] == 'certificates' && $pluginparams['event'] == 'used_certificates') {
			$a_certificates = &$config['installedpackages']['acme']['certificates']['item'];
			$result['pkgname'] = "Acme";
			$result['certificatelist'] = array();
			if (is_array($a_certificates)) {
				foreach ($a_certificates as $certificate) {
					$certificatename = $certificate["name"];
					$cert = lookup_cert_by_name($certificatename);
					$item = array();
					$cert = $cert['refid'];
					$item['usedby'] = "Acme - {$certificatename}";
					$result['certificatelist'][$cert][] = $item;
				}
			}
		}
		return $result;
	}

	/* Attempt to restart a remote service via XMLRPC with the default XMLRPC settings. */
	function acme_xmlrpc_restart_service($servicename = "webgui", $extras = array()) {
		require_once("xmlrpc_client.inc");
		$rpc_client = new pfsense_xmlrpc_client();
		$execstr = "";
		if ($servicename == "webgui") {
			/* The GUI needs a special event to restart, it is not a traditional service. */
			$execstr = "send_event('service restart webgui');";
		} else {
			$execstr = <<<EOD
require_once('service-utils.inc');
service_control_restart("{$servicename}", $extras);
EOD;
		}
		$resp = $rpc_client->xmlrpc_exec_php($execstr);
		if ($resp != null) {
			return true;
		}
		return false;
	}

	function acme_fixup_service_args($servicestring) {
		$args = explode(' ', $servicestring);
		$extras = array();
		switch ($args[0]) {
			case "openvpn":
				if (in_array($args[1], array("server", "client"))) {
					$extras['vpnmode'] = $args[1];
				} else {
					return;
				}
				if (is_numeric($args[2])) {
					$extras['id'] = $args[2];
				} else {
					return;
				}
				$vpnsvc = find_service_by_openvpn_vpnid($extras['id']);
				if (empty($vpnsvc)) {
					return;
				}
				break;
			case "captiveportal":
				if (is_validaliasname($args[1])) {
					$extras['zone'] = $args[1];
				} else {
					return;
				}
				$cpzone = find_service_by_cp_zone($extras['zone']);
				if (empty($cpzone)) {
					return;
				}
				break;
		}
		return array($args[0], $extras);
	}
}

namespace pfsense_pkg\acme {

/* include all configuration functions */
require_once("functions.inc");
require_once("pkg-utils.inc");
require_once("notices.inc");
require_once("util.inc");
require_once("acme_utils.inc");
require_once("acme_sh.inc");
require_once("acme/acme_serverconnectors.inc");

$d_acmeconfdirty_path = $g['varrun_path'] . "/acme.conf.dirty";
#region global array item definitions..
// <editor-fold desc="global array item definitions">

$a_enabledisable = array();
$a_enabledisable['enable'] = array('name' => 'Enabled');
$a_enabledisable['disable'] = array('name' => 'Disabled');

global $a_acmeserver;
$a_acmeserver = array();
$a_acmeserver['letsencrypt-staging'] = array('name' => "Let's Encrypt Staging (for TESTING purposes)",
    'url' => 'https://acme-staging.api.letsencrypt.org'
);
$a_acmeserver['letsencrypt-production'] = array('name' => "Let's Encrypt Production (Applies rate limits to certificate requests)",
    'url' => 'https://acme-v01.api.letsencrypt.org'
);
/*$a_acmeserver['dummy'] = array('name' => "dummy",
    'url' => 'https://example.org'
);*/

global $acme_domain_validation_method;
$acme_domain_validation_method = array();
$acme_domain_validation_method['webroot'] = array(name => "webroot local folder",
	'fields' => array(
		'folder' => array('name'=>"folder",'columnheader'=>"RootFolder",'type'=>"textbox",'size'=>"50",
			'description' =>"Folder the acme challenge response is written to; for example: /usr/local/www/.well-known/acme-challenge/"
		)
	));
$acme_domain_validation_method['webrootftp'] = array(name => "webroot FTP",
	'fields' => array(
		'ftpserver' => array('name'=>"ftpserver",'columnheader'=>"Server",'type'=>"textbox",'size'=>"50",
			'description' =>"Hostname of FTP server to connect to for example: ftps://www.webserver.tld "
			. "currently ftps(passive) and sftp are supported."
		),
		'username' => array('name'=>"username",'columnheader'=>"Username",'type'=>"textbox",'size'=>"50",
			'description' =>"Username for the remote server"
		),
		'password' => array('name'=>"password",'columnheader'=>"Password",'type'=>"textbox",'size'=>"50",
			'description' =>"Password used to authenticate to the server"
		),
		'folder' => array('name'=>"folder",'columnheader'=>"Folder",'type'=>"textbox",'size'=>"50",
			'description' =>"Folder the acme challenge response is written to for default: /.well-known/acme-challenge/"
		)
	));
$acme_domain_validation_method['standalone'] = array(name => "standalone HTTP server",
	'fields' => array(
		'port' => array('name'=>"port",'columnheader'=>"Port",'type'=>"textbox",'size'=>"50",
			'description' =>"HTTP listen port for stand-alone server. Must be 80 or have port 80 on WAN forwarded to this port. Firewall rules must also allow traffic to reach this port."
		)
	));
$acme_domain_validation_method['standalonetls'] = array(name => "standalone TLS server",
	'fields' => array(
		'port' => array('name'=>"port",'columnheader'=>"Port",'type'=>"textbox",'size'=>"50",
			'description' =>"TLS listen port for stand-alone server. Must be 443 or have port 443 on WAN forwarded to this port. Firewall rules must also allow traffic to reach this port."
		)
	));
$acme_domain_validation_method['dns_manual'] = array(name => "DNS-Manual",
	'fields' => array(
	));
$acme_domain_validation_method['dns_ali'] = array(name => "DNS-Aliyuncs",
	'fields' => array(
		'Ali_Key' => array('name'=>"ali_key",'columnheader'=>"Key",'type'=>"textbox",
			'description' =>"Fill in the API Key"
		),
		'Ali_Secret' => array('name'=>"ali_secret",'columnheader'=>"Secret",'type'=>"textbox",
			'description' =>"Fill in the API Secret"
		)
	));
$acme_domain_validation_method['dns_aws'] = array(name => "DNS-Amazon Route53",
	'fields' => array(
		'AWS_ACCESS_KEY_ID' => array('name'=>"aws_access_key_id",'columnheader'=>"Id",'type'=>"textbox",
			'description' =>"Fill in the API Id"
		),
		'AWS_SECRET_ACCESS_KEY' => array('name'=>"aws_secret_access_key",'columnheader'=>"Key",'type'=>"textbox",
			'description' =>"Fill in the API Key"
		)
	));
$acme_domain_validation_method['dns_cf'] = array(name => "DNS-Cloudflare",
	'fields' => array(
		'CF_Key' => array('name'=>"cf_key",'columnheader'=>"Key",'type'=>"textbox",
			'description' =>"Fill in the API Key"
		),
		'CF_Email' => array('name'=>"cf_email",'columnheader'=>"Email",'type'=>"textbox",
			'description' =>"Fill in the API Emailadress"
		)
	));
$acme_domain_validation_method['dns_cx'] = array(name => "DNS-Cloudxns",
	'fields' => array(
		'CX_Key' => array('name'=>"cx_key",'columnheader'=>"Key",'type'=>"textbox",
			'description' =>"Fill in the API Key"
		),
		'CX_Secret' => array('name'=>"cx_secret",'columnheader'=>"Secret",'type'=>"textbox",
			'description' =>"Fill in the API Secret"
		)
	));
$acme_domain_validation_method['dns_dp'] = array(name => "DNS-Dnspod.cn",
	'fields' => array(
		'DP_Id' => array('name'=>"dp_id",'columnheader'=>"Id",'type'=>"textbox",
			'description' =>"Fill in the API Id"
		),
		'DP_Key' => array('name'=>"dp_key",'columnheader'=>"Key",'type'=>"textbox",
			'description' =>"Fill in the API Key"
		)
	));
$acme_domain_validation_method['dns_gd'] = array(name => "DNS-Godaddy",
	'fields' => array(
		'GD_Key' => array('name'=>"gd_key",'columnheader'=>"Key",'type'=>"textbox",
			'description' =>"Fill in the API Key"
		),
		'GD_Secret' => array('name'=>"gd_secret",'columnheader'=>"Secret",'type'=>"textbox",
			'description' =>"Fill in the API Secret"
		)
	));
$acme_domain_validation_method['dns_ispconfig'] = array(name => "DNS-ISPConfig",
	'fields' => array(
		'ISPC_User' => array('name'=>"ispc_user",'columnheader'=>"User",'type'=>"textbox",
			'description' =>"Fill in the remoteUser"
		),
		'ISPC_Password' => array('name'=>"ispc_password",'columnheader'=>"Password",'type'=>"textbox",
			'description' =>"Fill in the remotePassword"
		),
		'ISPC_Api' => array('name'=>"ispc_api",'columnheader'=>"Api",'type'=>"textbox",
			'description' =>"Fill in the https://ispc.domain.tld:8080/remote/json.php"
		),
		'ISPC_Api_Insecure' => array('name'=>"ispc_api_insecure",'columnheader'=>"Secure",'type'=>"textbox",
			'description' =>"Fill in the Set 1 for insecure and 0 for secure  -> difference is whether ssl cert is checked for validity (0) or whether it is just accepted (1)"
		)
	));
/*
You must install python and lexicon before using it. ???
$acme_domain_validation_method['dns_lexicon'] = array(name => "DNS-Lexicon",
	'fields' => array(
		'' => array('name'=>"ftpserver",'columnheader'=>"Key",'type'=>"textbox",
			'description' =>"Fill in the API Key"
		),
		'' => array('name'=>"username",'columnheader'=>"Id",'type'=>"textbox",
			'description' =>"Fill in the API Id"
		)
	));*/
$acme_domain_validation_method['dns_luadns'] = array(name => "DNS-Luadns",
	'fields' => array(
		'LUA_Key' => array('name'=>"lua_key",'columnheader'=>"Key",'type'=>"textbox",
			'description' =>"Fill in the API Key"
		),
		'LUA_Email' => array('name'=>"lua_email",'columnheader'=>"Email",'type'=>"textbox",
			'description' =>"Fill in the API Email Address"
		)
	));
$acme_domain_validation_method['dns_me'] = array(name => "DNS-DNSMadeEasy",
	'fields' => array(
		'ME_Key' => array('name'=>"me_key",'columnheader'=>"Key",'type'=>"textbox",
			'description' =>"Fill in the API Key"
		),
		'ME_Secret' => array('name'=>"me_secret",'columnheader'=>"Secret",'type'=>"textbox",
			'description' =>"Fill in the API Secret"
		)
	));
$acme_domain_validation_method['dns_nsupdate'] = array(name => "DNS-NSupdate",
	'fields' => array(
		'NSUPDATE_SERVER' => array('name'=>"nsupdate_server",'columnheader'=>"Server",'type'=>"textbox",
			'description' =>"Fill in the nsupdate DNS server (IP address or hostname)"
		),
		'NSUPDATE_KEYTYPE' => array('name' => 'nsupdate_keytype', 'columnheader' => 'Key Type', 'type' => 'select',
			'items' => array(
				'host' => array('name' => 'Host Key'),
				'user' => array('name' => 'User Key'),
				'zone' => array('name' => 'Zone Key'),
			),
			'description' => 'Choose the nsupdate key type (typically "host")'
		),
		'NSUPDATE_KEYALGO' => array('name' => 'nsupdate_keyalgo', 'columnheader' => 'Key Algorithm', 'type' => 'select',
			'items' => array(
				'157' => array('name' => 'HMAC-MD5'),
				'165' => array('name' => 'HMAC-SHA512'),
				'164' => array('name' => 'HMAC-SHA384'),
				'163' => array('name' => 'HMAC-SHA256'),
				'162' => array('name' => 'HMAC-SHA224'),
				'161' => array('name' => 'HMAC-SHA1'),
			),
			'description' => 'Choose the nsupdate key algorithm  (typically "HMAC-MD5")'
		),
		'NSUPDATE_KEY' => array('name'=>"nsupdate_key",'columnheader'=>"Key",'type'=>"textarea",
			'description' =>"Fill in the nsupdate key contents"
		)
	));
$acme_domain_validation_method['dns_ovh'] = array(name => "DNS-ovh / kimsufi / soyoustart / runabove",
	'fields' => array(
		'OVH_AK' => array('name'=>"ovh_ak",'columnheader'=>"Application Key",'type'=>"textbox",
			'description' =>"Fill in the Application Key"
		),
		'OVH_AS' => array('name'=>"ovh_as",'columnheader'=>"Application Secret",'type'=>"textbox",
			'description' =>"Fill in the Application Secret"
		),
		'OVH_CK' => array('name'=>"ovh_ck",'columnheader'=>"Consumer Key",'type'=>"textbox",
			'description' =>"Fill in the Consumer Key"
		),
		'OVH_END_POINT' => array('name'=>"ovh_end_point",'columnheader'=>"Endpoint",'type'=>"textbox",
			'description' =>"Fill in one of: ovh-eu/ovh-ca/kimsufi-eu/kimsufi-ca/soyoustart-eu/soyoustart-ca/runabove-ca"
		)
	));
$acme_domain_validation_method['dns_pdns'] = array(name => "DNS-PowerDNS",
	'fields' => array(
		'PDNS_Url' => array('name'=>"pdns_url",'columnheader'=>"URL",'type'=>"textbox",
			'description' =>"Fill in the URL http://ns.example.com:8081"
		),
		'PDNS_ServerId' => array('name'=>"pdns_serverid",'columnheader'=>"ServerID",'type'=>"textbox",
			'description' =>"Fill in the ServerId localhost"
		),
		'PDNS_Token' => array('name'=>"pdns_token",'columnheader'=>"Token",'type'=>"textbox",
			'description' =>"Fill in the Token 0123456789ABCDEF"
		),
		'PDNS_Ttl' => array('name'=>"pdns_ttl",'columnheader'=>"TTL",'type'=>"textbox",
			'description' =>"Fill in the TTL 60"
		)
	));
$acme_domain_validation_method['dns_he'] = array(name => "DNS-Hurricane Electric",
	'fields' => array(
		'HE_Username' => array('name'=>"he_username",'columnheader'=>"Username",'type'=>"textbox",
			'description' =>"Hurricane Electric username"
		),
		'HE_Password' => array('name'=>"he_password",'columnheader'=>"Password",'type'=>"textbox",
			'description' =>"Hurricane Electric password"
		)
	));
//TODO add more challenge validation types
/* 
$acme_domain_validation_method['http-post'] = array(name => "http-post",
	'fields' => array(
		'url' => array('name'=>"url",'columnheader'=>"Url",'type'=>"textbox",'size'=>"50",
			'description' =>"Url the challenge file is posted to, the webserver there must store and reply to the request when the acme servers perform the request for the file from /.well-known/acme-challenge/"
		)
	));
*/

//TODO add more 'actions'
$acme_newcertificateactions = array(
	'shellcommand' => array(name => "Shell Command"),
	'php_command' => array(name => "PHP Command Script"),
	'servicerestart' => array(name => "Restart Local Service"),
	'xmlrpcservicerestart' => array(name => "Restart Remote Service (XMLRPC)"),
);

// </editor-fold>
#end

$a_keylength = array();
$a_keylength['2048'] = array(name => "2048");
$a_keylength['3072'] = array(name => "3072");
$a_keylength['4096'] = array(name => "4096");
$a_keylength['ec-256'] = array(name => "ec-256", ecc => true);
$a_keylength['ec-384'] = array(name => "ec-384", ecc => true);

function set_cronjob() {
	global $config;
	$a_global = &$config['installedpackages']['acme'];
	if (isset($a_global['enable'])) {
		install_cron_job('/usr/local/pkg/acme/acme_command.sh "renewall"', true, "16", "3");
	} else {
		install_cron_job('/usr/local/pkg/acme/acme_command.sh "renewall"', false);
	}
}

function acme_custom_php_deinstall_command() {
	global $static_output;
	$static_output .= "Acme, running acme_custom_php_deinstall_command()\n";
	update_output_window($static_output);
	$static_output .= "Acme, uninstalling cron job\n";
	update_output_window($static_output);
	install_cron_job("/etc/rc.acme_renew.sh", false);
	$static_output .= "Acme, running acme_custom_php_deinstall_command() DONE\n";
	update_output_window($static_output);
}

function acme_custom_php_install_command() {
	global $static_output;
	$static_output .= "Acme, running acme_custom_php_install_command()\n";
	update_output_window($static_output);
	$static_output .= "Acme, installing cron job if enabled\n";
	update_output_window($static_output);
	set_cronjob();
	$static_output .= "Acme, running acme_custom_php_install_command() DONE\n";
	update_output_window($static_output);
}

function get_itembyname($a_array, $name) {
	$i = 0;
	if (is_array($a_array)) {
		foreach ($a_array as $item) {
			if ($item['name'] == $name) {
				return $i;
			}
			$i++;
		}
	}
	return null;	
}
	
function get_accountkey_id($name) {
	global $config;
	$a_array = &$config['installedpackages']['acme']['accountkeys']['item'];
	return get_itembyname($a_array, $name);
}
function get_accountkey($name) {
	global $config;
	$a_array = &$config['installedpackages']['acme']['accountkeys']['item'];
	$id = get_accountkey_id($name);
	return $a_array[$id];
}

function get_certificate_id($name) {
	global $config;
	$a_certificates = &$config['installedpackages']['acme']['certificates']['item'];
	$i = 0;
	if (is_array($a_certificates)) {
		foreach ($a_certificates as $certificate) {
			if ($certificate['name'] == $name) {
				return $i;
			}
			$i++;
		}
	}
	return null;
}

function & get_certificate($name) {
	global $config;
	$a_certificates = &$config['installedpackages']['acme']['certificates']['item'];
	$id = get_certificate_id($name);
	if (is_numeric($id)) {
		return $a_certificates[$id];
	}
	return null;
}

	function generateAccountKey($name, $ca) {
		$acmesh = new acme_sh($name, $ca);
		return $acmesh->generateAccountKey();
	}
	
	function generateDomainKey($name, $ca, $domain, $keylength) {
		$acmesh = new acme_sh($name, $ca);
		return $acmesh->generateDomainKey($domain, $keylength);
	}
	
	function registerAcmeAccountKey($name, $ca, $key) {
		$acmesh = new acme_sh($name, $ca);
		return $acmesh->registeraccount($key);
	}

	function renew_all_certificates() {
		global $config;
		$a_global = &$config['installedpackages']['acme'];
		if (is_array($a_global['certificates']['item'])) {
			foreach($a_global['certificates']['item'] as $certificate) {
				echo "Checking if renewal is needed for: {$certificate['name']}\n";
				issue_certificate($certificate['name']);
			}
		}
	}
	
	function getCertificatePSK($ca, $certificate, $domain) {
		$certificatename = $certificate['name'];
		$cert = lookup_cert_by_name($certificatename);
		if (!is_array($cert)) {
			echo "\n getCertificatePSK creating new cert";
			global $config;
			$a_cert =& $config['cert'];
			$cert = array();
			$cert['refid'] = uniqid();
			$cert['descr'] = $certificatename;
			$accountkey = generateDomainKey($certificatename, $ca, $domain, $certificate['keylength']);
			$cert['prv'] = base64_encode($accountkey);
			$a_cert[] = $cert;
			echo "\n{$cert['prv']}";
			$desc = "Acme: Add new certificate & key.";
			write_config($desc);
		}
		return base64_decode($cert['prv']);
	}
	
	function saveCACertificateToStore($crt) {
		global $config;
		$crt_enc = base64_encode($crt);
		if (!is_array($config['ca'])) {
			$config['ca'] = array();
		}
		$a_ca =& $config['ca'];
		foreach($a_ca as $ca) {
			if ($ca['crt'] == $crt_enc) {
				return;
			}
		}
		$subject = cert_get_subject($crt, false);
		$cert = array();
		$cert['refid'] = uniqid();
		$cert['descr'] = "Acmecert: {$subject}";
		ca_import($cert, $crt);
		$a_ca[] = $cert;
		syslog(LOG_NOTICE, "Acme, storing new CA: {$subject}");
	}
	
	function storeCertificateCer($certificatename, $keyfile, $cerfile, $fullchainfile) {
		global $config;
		$certupdated = false;
		
		$key = file_get_contents($keyfile);
		$crt = file_get_contents($cerfile);
		$fullchain = "";
		if(!empty($fullchainfile)) {
			$fullchain = file_get_contents($fullchainfile);
			preg_match_all("/-+BEGIN CERTIFICATE(.+?)END CERTIFICATE-+/s", $fullchain, $certificatematches);
			$first = true;
			foreach($certificatematches[0] as $cacert) {
				if ($first == true) {
					$first = false;
					continue;
				}
				saveCACertificateToStore($cacert);
			}
		}
		if (is_array($config['cert'])) {
			foreach ($config['cert'] as &$cert) {
				if ($cert['descr'] == $certificatename) {
					syslog(LOG_NOTICE, "Acme, storing new certificate: {$certificatename}");
					echo "\nupdate cert!";
					cert_import($cert, $crt, $key);
					$certupdated = true;
					break;
				}
			}
		}
		
		return $certupdated;
	}
		
	function issue_certificate($id, $force = false, $renew = false) {
		$certificate = & get_certificate($id);
		if (!$force) {
			if ($certificate['status'] != 'active') {
				echo "Certificate renewal for this certificate is set to: disabled\n";
				return;
			}

			$renewafterdays = is_numericint($certificate['renewafter']) ? $certificate['renewafter'] : 60;	
			$timetorenew = false;
			$now = new \DateTime();
			$lastrenewal = new \DateTime();
			$lastrenewal->setTimestamp($certificate['lastrenewal']);
			$nextrenewal = $lastrenewal->add(new \DateInterval('P'.$renewafterdays.'D'));
			if ($now >= $nextrenewal) {
				echo "## Its time to renew ##\n";
				$timetorenew = true;
			} else {
				echo "Renewal number of days not yet reached.\n";
			}
		}
		
		if ($timetorenew || $force) {
			syslog(LOG_NOTICE, "Acme, renewing certificate: {$id}");
			echo "Renewing certificate";
			$domainstosign = array();
			$method = "";
			if (is_array($certificate['a_domainlist']['item'])) {
				foreach($certificate['a_domainlist']['item'] as $domain) {
					if ($domain['status'] == 'disable') {
						continue;
					}
					$domainstosign[] = $domain['name'];
					$method = $domain['method'];
					$envvariables = array();
					global $acme_domain_validation_method;
					foreach($acme_domain_validation_method[$method]['fields'] as $key => $field) {
						$envvariables[$key] = $domain["{$method}{$field['name']}"];
						if ($method == "dns_nsupdate") {
							$extras[$domain['name']][$key] = $domain["{$method}{$field['name']}"];
						}
					}
				}
			}
			
			echo "account: {$certificate['acmeaccount']} \n";
			$account = get_accountkey($certificate['acmeaccount']);
			$acmeserver = $account['acmeserver'];
			$accountkey = $account['accountkey'];
			echo "server: $acmeserver \n";
			
			global $a_acmeserver;
			$url = $a_acmeserver[$acmeserver]['url'];
			$certificatepsk = getCertificatePSK($url, $certificate, $domainstosign[0]);
			$acmesh = new acme_sh($certificate['name'], $url);
			$action = $renew == true ? "renew" : "issue";
			$acmesh->dnssleep = $certificate['dnssleep'];
			$acmesh->signCertificate($action, $accountkey, $certificatepsk, $domainstosign, $method, $envvariables, $extras);
		}
	}
	
	function challenge_response_put($certificatename, $domain, $token, $payload) {
		$acmecert = get_certificate($certificatename);
		global $ftpconn;
		
		echo "\nchallenge_response_put $certificatename, $domain\n";
		foreach($acmecert['a_domainlist']['item'] as $domainitem) {
			if($domainitem['name'] == $domain){
				$domain_info = $domainitem;
				echo "FOUND domainitem";
			}
		}
		if ($domain_info['method'] == 'webroot') {
			echo "webroot\n";
			$directory = $domain_info['webrootfolder'];
			if(!file_exists($directory) && !@mkdir($directory, 0755, true)) {
				throw new \RuntimeException("Couldn't create directory to expose challenge: ${tokenPath}");
			}
			$tokenPath = $directory . "/" . $token;
			file_put_contents($tokenPath, $payload);
			echo "put token at: {$tokenPath}\n";
		}
		if ($domain_info['method'] == 'webrootftp') {
			echo "FTP\n";
			try {
				$ftpconn = new FTPConnection($domain_info['webrootftpftpserver']);
				$ftpconn->login($domain_info['webrootftpusername'], $domain_info['webrootftppassword']);
				$directory = $domain_info['webrootftpfolder'];
				$tokenPath = $directory . "/" . $token;
				$ftpconn->mkdir($directory);
				$ftpconn->uploadData($payload, $tokenPath);
			} catch (\Exception $e) {
				echo 'FTP Attempt Failed: ',  $e->getMessage(), "\n";
			}
		}
	}	

	function challenge_response_cleanup($certificatename, $domain, $token) {
		global $ftpconn;
		$acmecert = get_certificate($certificatename);
		foreach($acmecert['a_domainlist']['item'] as $domainitem) {
			if($domainitem['name'] == $domain){
				$domain_info = $domainitem;
			}
		}
		if ($domain_info['method'] == 'webroot') {
			$tokenfile = $domain_info['webrootfolder'] . "/" . $token;
			@unlink($tokenfile);
		}
		if ($domain_info['method'] == 'webrootftp') {
			$tokenfile = $domain_info['webrootftpfolder'] . "/" . $token;
			if ($ftpconn) {
				try {
					$ftpconn->deleteFile($tokenfile);
				} catch (\Exception $e) {
					echo 'FTP Attempt Failed: ',  $e->getMessage(), "\n";
				}
			}
		}
	}
}
