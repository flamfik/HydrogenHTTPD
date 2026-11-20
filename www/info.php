<?php
// Minimal FastCGI smoke-test endpoint. Do not expose runtime or environment details.
header("Content-Type: text/plain; charset=utf-8");
echo "HydrogenHttpd PHP-FPM integration OK\n";
?>
