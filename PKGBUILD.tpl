# Maintainer: Paul Colomiets <paul@colomiets.name>

pkgname=procboss
pkgver=${VERSION}
pkgrel=1
pkgdesc="A process supervisor"
arch=('i686' 'x86_64')
url="http://github.com/tailhook/procboss"
license=('GPL')
depends=('coyaml')
makedepends=('coyaml' 'docutils')
backup=("etc/bossd.yaml")
source=(https://github.com/downloads/tailhook/procboss/$pkgname-$pkgver.tar.gz)
md5sums=('${DIST_MD5}')

build() {
  cd $srcdir/$pkgname-$pkgver
  ./waf configure --prefix=/usr
  ./waf build
}

package() {
  cd $srcdir/$pkgname-$pkgver
  ./waf install --destdir=$pkgdir
  install -D $pkgdir/var/log/boss
  install -D $pkgdir/var/run/boss
}
