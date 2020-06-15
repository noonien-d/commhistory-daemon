###############################################################################
#
# This file is part of commhistory-daemon.
#
# Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
# Contact: Reto Zingg <reto.zingg@nokia.com>
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License version 2.1 as
# published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
#
###############################################################################

#-----------------------------------------------------------------------------
# Common installation configuration for all projects.
#-----------------------------------------------------------------------------


#-----------------------------------------------------------------------------
# setup the installation prefix
#-----------------------------------------------------------------------------
INSTALL_PREFIX = /usr  # default installation prefix

# default prefix can be overriden by defining PREFIX when running qmake
!isEmpty( PREFIX ) {
    INSTALL_PREFIX = $${PREFIX}
    message("==== install prefix set to `$${INSTALL_PREFIX}'")
}


#-----------------------------------------------------------------------------
# default installation target for applications
#-----------------------------------------------------------------------------
contains( TEMPLATE, app ) {
    target.path  = $${INSTALL_PREFIX}/bin
    INSTALLS    += target
}

#-----------------------------------------------------------------------------
# default installation target for libraries
#-----------------------------------------------------------------------------
contains( TEMPLATE, lib ) {

    target.path  = $$[QT_INSTALL_LIBS]
    INSTALLS    += target


    #-------------------------------------------------------------------------
    # target for pkg-config file
    #-------------------------------------------------------------------------
    CONFIG *= create_prl create_pc
    pkgconfig.files  = $${TARGET}.pc
    pkgconfig.path   = $$[QT_INSTALL_LIBS]/pkgconfig
    INSTALLS        += pkgconfig

    # reset the .pc file's `prefix' variable
    include( tools/fix-pc-prefix.pri )

}

#-----------------------------------------------------------------------------
# target for header files
#-----------------------------------------------------------------------------
!isEmpty( headers.files ) {
    headers.path  = $${INSTALL_PREFIX}/include/$${TARGET}
    INSTALLS     += headers
}


# End of File
