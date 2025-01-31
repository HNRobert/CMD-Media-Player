sudo apt-get update
sudo apt-get install build-essential

/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# export PATH="/home/linuxbrew/.linuxbrew/bin:$PATH"

# Configure Homebrew environment
echo 'eval "$(/home/linuxbrew/.linuxbrew/bin/brew shellenv)"' >> /root/.bashrc

echo 'export LD_LIBRARY_PATH=$HOMEBREW_PREFIX/lib:$LD_LIBRARY_PATH' >> /root/.bashrc
echo 'export PKG_CONFIG_PATH=$HOMEBREW_PREFIX/lib/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig:$PKG_CONFIG_PATH' >> /root/.bashrc

source ~/.bashrc

brew -v

brew tap hnrobert/cmdp && brew install cmdp
