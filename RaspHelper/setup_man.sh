touch /home/hibic/.zshrc

sh -c "$(wget https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh -O -)"

echo 'export DOTNET_ROOT=$HOME/.dotnet' >> /home/hibic/.zshrc
echo 'export PATH=$PATH:$HOME/.dotnet' >> /home/hibic/.zshrc
echo 'alias dmo="python $HOME/code/SelfTools/RaspHelper/device_monitor.py"' >> /home/hibic/.zshrc
zsh -c 'source /home/hibic/.zshrc'

# sudo chsh -s /usr/bin/zsh