/var/log/boa/*_log {
  rotate 7
  daily
  compress
  copytruncate
  missingok
  notifempty
}
