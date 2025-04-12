sudo apt-get update
sudo apt-get -y upgrade
sudo apt-get install openssh-server g++ gdb make ninja-build rsync zip git zsh pip stress vulkan-tools libvulkan-dev vulkan-validationlayers spirv-tools libxxf86vm-dev libxi-dev libglfw3-dev libglm-dev glslc -y
curl -sSL https://dot.net/v1/dotnet-install.sh | bash /dev/stdin --channel LTS
curl -sSL https://dot.net/v1/dotnet-install.sh | bash /dev/stdin --channel STS

git config --global user.name "Cai Yuanhan"
git config --global user.email cai_yuanhan@outlook.com

sudo rm -rf /home/hibic/code
mkdir /home/hibic/code
cd /home/hibic/code
git clone https://github.com/HIBICUS-CAI/SelfTools.git