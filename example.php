<?php
$rootdir = pwd();
$dir = $_REQUEST['d'];

// Some very basic sanitizing, otherwise people could download random paths off the server.
// For best security, you would want to use $dir as an index in a list rather than trusting it at all.
if (strpos($dir, "..") !== false) die("Invalid directory.");
$realroot = realpath($rootdir);
$realdir = realpath("$rootdir/$dir");
if (strpos($realdir, $realroot) === false) die("Invalid directory.");
if (!is_dir($realdir)) die("Invalid directory.");

chdir($rootdir);

$cmd = [escapeshellcmd("$rootdir/zipstream-deflate")];

function globfiles($path) {
	global $cmd;

	$files = glob("$path/*", GLOB_NOESCAPE);
	foreach ($files as $file) {
		if (is_dir($file)) {
			globfiles($file);
		}
		$cmd[] = escapeshellarg($file);
	}
}
globfiles(".");

header('Content-Type: application/zip');
header('Content-Disposition: attachment; filename="'.basename($dir).'.zip"');

set_time_limit(0);
passthru(implode(" ", $cmd));
