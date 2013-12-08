# Copyright (C) 2013 Bartosz Golaszewski <bartekgola@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.

"""
Test program output in case of invalid arguments.
"""

import libregr

def run():
	libregr.callExpect('call', retcode=1,
				stderr='./bbus-call: expected additional '
					'parameters\ntry ./bbus-call --help')
