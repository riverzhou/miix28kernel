

sudo cp -rf bytcr-rt5640 /usr/share/alsa/ucm/

alsactl store && alsactl restore

sudo cp asound.state /var/lib/alsa/

alsactl restore



