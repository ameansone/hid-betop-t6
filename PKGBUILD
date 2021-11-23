_pkgbase=hid-betop-t6
pkgname=${_pkgbase}-dkms
pkgver=0.1
pkgrel=1
pkgdesc="hid-betop-t6 Driver (DKMS)"
arch=('any')
url="https://github.com/ameansone/hid-betop-t6"
license=('GPLv2')
depends=('dkms' 'bc')
provides=('hid-betop-t6')
source=("hid-betop-t6.c"
        "hid-ids.h"
        "Makefile"
        "dkms.conf")
md5sums=('5a55f98885bc0189304620bf9b645f78'
         '4d0a7cbb61630422f15595f61b435d44'
         '03461c6f16fcd87e0e88429add2cac75'
         'bd36861eebd9ba173514dbfb0ef57f5e')

package() {
  mkdir -p "${pkgdir}"/usr/src/${_pkgbase}-${pkgver}/
  chmod 755 "${pkgdir}"/usr/src/${_pkgbase}-${pkgver}/

  install -Dm644 ${srcdir}/* "${pkgdir}"/usr/src/${_pkgbase}-${pkgver}/
}
