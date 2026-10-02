#!/bin/sh
# Build a signed APT repository tree from directories of .deb files.
#
#   build-repo.sh <stable-debs> <unstable-debs> <out-dir>
#
# Writes <out-dir>/{pool,dists}/{stable,unstable}, pubkey.gpg, pubkey.asc and index.html.
# Stateless: the tree is rebuilt from scratch from the .debs given, so the repository can never
# drift from what is on the GitHub Releases page.
#
# Environment:
#   GPG_KEY_ID          key used to sign Release (required)
#   GPG_PASSPHRASE_FILE file with the key's passphrase (optional)
#   REPO_URL            public URL, only used in index.html (optional)
set -eu

[ $# -eq 3 ] || { echo "usage: $0 <stable-debs> <unstable-debs> <out-dir>" >&2; exit 2; }
stable_dir=$1 unstable_dir=$2 out=$3
: "${GPG_KEY_ID:?GPG_KEY_ID is required}"
REPO_URL=${REPO_URL:-https://example.invalid/bluedragon}
ARCHES="amd64 arm64"

gpg_sign() {   # gpg_sign <extra gpg args...>
    if [ -n "${GPG_PASSPHRASE_FILE:-}" ]; then
        gpg --batch --yes --pinentry-mode loopback --passphrase-file "$GPG_PASSPHRASE_FILE" \
            --local-user "$GPG_KEY_ID" "$@"
    else
        gpg --batch --yes --local-user "$GPG_KEY_ID" "$@"
    fi
}

rm -rf "$out"
mkdir -p "$out"

build_dist() {   # build_dist <distribution> <deb dir> <description>
    dist=$1 src=$2 desc=$3
    mkdir -p "$out/pool/$dist"
    if [ -d "$src" ]; then
        find "$src" -maxdepth 1 -name '*.deb' -exec cp {} "$out/pool/$dist/" \;
    fi
    for arch in $ARCHES; do
        d="$out/dists/$dist/main/binary-$arch"
        mkdir -p "$d"
        ( cd "$out" && apt-ftparchive --arch "$arch" packages "pool/$dist" ) > "$d/Packages"
        gzip -9nc "$d/Packages" > "$d/Packages.gz"
    done
    ( cd "$out" && apt-ftparchive \
        -o "APT::FTPArchive::Release::Origin=bluedragon" \
        -o "APT::FTPArchive::Release::Label=bluedragon" \
        -o "APT::FTPArchive::Release::Suite=$dist" \
        -o "APT::FTPArchive::Release::Codename=$dist" \
        -o "APT::FTPArchive::Release::Architectures=$ARCHES" \
        -o "APT::FTPArchive::Release::Components=main" \
        -o "APT::FTPArchive::Release::Description=$desc" \
        release "dists/$dist" ) > "$out/dists/$dist/Release"
    gpg_sign --clearsign -o "$out/dists/$dist/InRelease" "$out/dists/$dist/Release"
    gpg_sign --armor --detach-sign -o "$out/dists/$dist/Release.gpg" "$out/dists/$dist/Release"
}

build_dist stable   "$stable_dir"   "bluedragon: tagged releases"
build_dist unstable "$unstable_dir" "bluedragon: latest build of main (may be broken)"

gpg --batch --yes --export "$GPG_KEY_ID" > "$out/pubkey.gpg"
gpg --batch --yes --armor --export "$GPG_KEY_ID" > "$out/pubkey.asc"

cat > "$out/index.html" <<HTML
<!doctype html>
<meta charset="utf-8">
<title>bluedragon APT repository</title>
<body style="font-family: sans-serif; max-width: 46em; margin: 2em auto; padding: 0 1em">
<h1>bluedragon APT repository</h1>
<p>Packages for the M711 gaming mouse: <code>libbluedragon0</code>, <code>libbluedragon-dev</code>,
<code>bluedragon</code> (command line) and <code>bluedragon-gui</code>. Built for Ubuntu 24.04 and newer
(amd64, arm64).</p>
<pre>sudo curl -fsSLo /usr/share/keyrings/bluedragon.gpg $REPO_URL/pubkey.gpg
echo "deb [signed-by=/usr/share/keyrings/bluedragon.gpg] $REPO_URL stable main" | sudo tee /etc/apt/sources.list.d/bluedragon.list
sudo apt update
sudo apt install bluedragon-gui</pre>
<p>Use <code>unstable</code> instead of <code>stable</code> for the latest build of <code>main</code>.</p>
<p>Source: <a href="https://github.com/geovannyAvelar/bluedragon">github.com/geovannyAvelar/bluedragon</a></p>
HTML
