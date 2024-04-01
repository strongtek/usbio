# Install libft4222 and headers into /usr/local, so run using sudo!

if [ "$(id -u)" != "0" ] ; then
	echo "You must run this script with sudo."
	exit 1
fi

# Inside libft4222.tgz are release/32 and release/64 directories.
pathToLib=$(ls build/libft4222.1.*)
# echo "Found $pathToLib"
Lib=$(basename $pathToLib)
echo "Copying $pathToLib to /usr/local/lib"
yes | cp "$pathToLib" /usr/local/lib

pathToLib=$(ls build/libftd2xx.dylib)
# echo "Found $pathToLib"
echo "Copying $pathToLib to /usr/local/lib"
yes | cp "$pathToLib" /usr/local/lib

# Remove any existing symlink for libft4222
rm -f /usr/local/lib/libft4222.dylib
ln -sf /usr/local/lib/$Lib /usr/local/lib/libft4222.dylib
ls -l /usr/local/lib/libft4222*

echo "Copying headers to /usr/local/include"
cp libft4222.h /usr/local/include
cp ftd2xx.h /usr/local/include
cp WinTypes.h /usr/local/include
ls -l /usr/local/include/*.h

