
# Disable Spotlight indexing
sudo mdutil -a -i off
sudo fs_usage  -w -f filesys mds_stores

# Clear out Spotlight's metadata store
sudo rm -rf /System/Volumes/Data/.Spotlight-V100/*