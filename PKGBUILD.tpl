# Maintainer: Paul Colomiets <paul@colomiets.name>

pkgname=procboss
pkgver=${VERSION}
pkgrel=1
pkgdesc="A process supervisor"
arch=('i686' 'x86_64')
url="http://github.com/tailhook/procboss"
license=('MIT')
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
  install -d -m755 $pkgdir/var/log/boss
  install -d -m755 $pkgdir/var/run/boss
  install -d -m755 $pkgdir/usr/share/procboss/examples
  install -m644 ./examples/services/* "$pkgdir/usr/share/$pkgname/examples"
  install -D -m644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
