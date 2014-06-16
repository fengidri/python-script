#encoding:utf8
import pyvim

from frain_libs import mitems 
from frain_libs import paths_exp
from frain_libs import data

from frain_libs import mescin
import flog


class FrainOpen( pyvim.command  ):
    """
        打开工程选择器. 选择工程并打开
    """
    def run( self ):
        mitems.frain_open_sel( )
        data.mode = True



class FrainOpenDir( pyvim.command ):
    def run( self ):
        if not self.params:
            return
        flog.loginfo( "FrainOpenDir:path:%s"% self.params )
        mitems.frain_open_dir( self.params[0] )
        data.mode = True

    def setting( self ):
        self.set_complete( self.complete_file )

class FrainClose( pyvim.events ):
    def on_VimLeavePre( self ):
        if not data.mode:
            return
        mitems.close( )




#############################
# path exp
#############################
class PathsExp( pyvim.command ):
    def run( self ):
        paths_exp.refresh_nodes( )
        paths_exp.refresh_win( )

class PathsExpFilter( pyvim.command ):
    def run( self ):
        paths_exp.blacklist_switch = not paths_exp.blacklist_switch
        paths_exp.refresh_nodes( )
        paths_exp.refresh_win( )

class PathsExpRefresh( pyvim.command ):
    def run( self ):
        paths_exp.refresh_nodes( )
        paths_exp.refresh_win( )

class PathsExpOpen( pyvim.command ):
    def run( self ):
        paths_exp.paths_exp_open( )

class PathsExpSwitch( pyvim.command ):
    def run( self ):
        paths_exp.switch_file_quick( )

class PathsExpFind( pyvim.command ):
    def run( self ):
        path = pyvim.vim.current.buffer.name
        if not path:
            return
        if pyvim.vim.current.buffer.options['buftype'] == 'nofile':
            return 
        if paths_exp.goto_path_exp_win( ):
            paths_exp.open_to_file( path )















