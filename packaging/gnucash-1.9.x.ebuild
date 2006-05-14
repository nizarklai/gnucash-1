# Copyright 1999-2005 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

# This script should work fine for the whole 1.9.x (and hopefully 2.0.x)
# releases with a simple rename. See
# http://bugs.gentoo.org/show_bug.cgi?id=122337 for discussion and history
# about this file.  

# As well, it'll work for as an SVN ebuild; drop it into
# ${PORTAGE_OVERLAY}/app-office/gnucash-svn/gnucash-svn-20060306.ebuild.
# It'll install into /opt/gnucash-svn-20060306/.

# -- jsled-gentoo@asynchronous.org

inherit eutils gnome2 

DESCRIPTION="A personal finance manager (unstable version)."
HOMEPAGE="http://www.gnucash.org/"
SRC_URI=""
if [[ "${PN}" != *-svn ]]; then
	SRC_URI="mirror://sourceforge/gnucash/${P}.tar.gz"
fi
LICENSE="GPL-2"
SLOT="1"
KEYWORDS="~amd64 ~x86"
IUSE="postgres ofx hbci chipcard doc debug quotes"
# mt940 qof

RDEPEND=">=dev-libs/glib-2.4.0
	>=dev-util/guile-1.6
	amd64? ( >=dev-util/guile-1.6.4-r2 )
	>=dev-libs/slib-2.3.8
	>=sys-libs/zlib-1.1.4
	>=dev-libs/popt-1.5
	>=x11-libs/gtk+-2.4
	>=gnome-base/libgnomeui-2.4
	>=gnome-base/libgnomeprint-2.10
	>=gnome-base/libgnomeprintui-2.10
	>=gnome-base/libglade-2.4
	>=gnome-extra/gtkhtml-3.6
	>=dev-libs/libxml2-2.5.10
	>=dev-libs/g-wrap-1.3.4
	>=gnome-base/gconf-2
	>=app-text/scrollkeeper-0.3
	>=x11-libs/goffice-0.0.4
	doc? ( app-doc/doxygen )
	ofx? ( >=dev-libs/libofx-0.7.0 )
	hbci? ( net-libs/aqbanking
		chipcard? ( sys-libs/libchipcard )
	)
	quotes? ( dev-perl/DateManip
		dev-perl/Finance-Quote
		dev-perl/HTML-TableExtract )
	postgres? ( dev-db/postgresql )"

# See http://bugs.gentoo.org/show_bug.cgi?id=118517
#	qof? ( >=qof-0.6.1 )
# or... same pattern as above?

# I [jsled-gentoo@asynchronous.org] don't think these are used by gnucash;
# maybe -docs...
#	app-text/docbook-xsl-stylesheets
#	=app-text/docbook-xml-dtd-4.1.2

DEPEND="${RDEPEND}
	dev-util/pkgconfig
	nls? ( sys-devel/gettext )"

pkg_setup() {
	built_with_use libgsf gnome || die "libgsf must be built with gnome"
	built_with_use goffice gnome || die "goffice must be built with gnome"
}

src_unpack() {
	if [[ "${PN}" == *-svn ]]; then
		svn co http://svn.gnucash.org/repo/gnucash/trunk ${P}
		cd ${S}
		./autogen.sh || die "Cannot autogen."
	else
		unpack ${A}
		cd ${S}
	fi
}

src_compile() {
	local myconf_warnings="--enable-error-on-warning --enable-compile-warnings"

	# We'd like to only define --prefix, but the econf definition seems
	# to check, but then promptly forget, that we've redefined it. :p
	# Thus, set {man,info,data,sysconf,localstate}dir too.
	econf \
		--prefix /opt/${P} \
		--mandir=/opt/${P}/man \
		--infodir=/opt/${P}/info \
		--datadir=/opt/${P}/share \
		--sysconfdir=/opt/${P}/etc \
		--localstatedir=/opt/${P}/var/lib \
		$(use_enable debug) \
		$(use_enable postgres sql) \
		$(use_enable ofx) \
		$(use_enable doc doxygen) \
		$(use_enable hbci) \
		${myconf_warnings} \
			|| die "econf failed"
	emake -j1 || die "emake failed"
}

# copied+mods from gnome2.eclass:
gnome2_gconf_install() {
	if [[ -x ${GCONFTOOL_BIN} ]]; then
		# We are ready to install the GCONF Scheme now
		unset GCONF_DISABLE_MAKEFILE_SCHEMA_INSTALL
		export GCONF_CONFIG_SOURCE=$(${GCONFTOOL_BIN} --get-default-source)

		einfo "Installing GNOME 2 GConf schemas"

		local contents="${ROOT}/var/db/pkg/*/${PN}-${PVR}/CONTENTS"

		for F in $(grep "obj /opt/${P}/etc/gconf/schemas" ${contents} | sed 's:obj \([^ ]*\) .*:\1:' ); do
			if [[ -e "${F}" ]]; then
				# echo "DEBUG::gconf install  ${F}"
				${GCONFTOOL_BIN} --makefile-install-rule ${F} 1>/dev/null
			fi
		done

		for user in `ps axuwwf | grep "gconfd-2" |grep -v "grep" | awk '{ print $1 }' | uniq`; do
			einfo "shutting down gconfd-2 for user ${user} to sync gnucash schemas"
			echo su ${user} -c "${GCONFTOOL_BIN} --shutdown"
			${ROOT}/bin/su - ${user} -c "${GCONFTOOL_BIN} --shutdown"
		done
	fi
}

# copied+mods from gnome2.eclass:
gnome2_gconf_uninstall() {
	if [[ -x ${GCONFTOOL_BIN} ]]; then
		unset GCONF_DISABLE_MAKEFILE_SCHEMA_INSTALL
		export GCONF_CONFIG_SOURCE=$(${GCONFTOOL_BIN} --get-default-source)

		einfo "Uninstalling GNOME 2 GConf schemas"

		local contents="${ROOT}/var/db/pkg/*/${PN}-${PVR}/CONTENTS"

		for F in $(grep "obj /opt/${P}/etc/gconf/schemas" ${contents} | sed 's:obj \([^ ]*\) .*:\1:' ); do
			# echo "DEBUG::gconf install  ${F}"
			${GCONFTOOL_BIN} --makefile-uninstall-rule ${F} 1>/dev/null
		done
	fi
}

src_install() {
	gnome2_src_install || die "gnome2_src_install failed"
	dodoc AUTHORS ChangeLog* DOCUMENTERS HACKING INSTALL LICENSE NEWS TODO README* doc/README*
	dodir /usr/bin
	dosym /opt/${P}/bin/gnucash /usr/bin/${P}
	make_desktop_entry ${P} "GnuCash ${PV}" \
		/opt/${P}/share/gnucash/pixmaps/appicon.png "Office;Finance"
}
