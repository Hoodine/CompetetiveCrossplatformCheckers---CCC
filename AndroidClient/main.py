import os

os.environ['KIVY_CLOCK'] = 'default'

from kivy.app import App
from kivy.uix.screenmanager import ScreenManager
from kivy.core.window import Window

Window.fullscreen = 'auto'
Window.maximize()


class CheckersApp(App):
    def build(self):
        from menu_screen import MenuScreen
        from game_screen import GameScreen

        sm = ScreenManager()
        sm.add_widget(MenuScreen(name='menu'))
        sm.add_widget(GameScreen(name='game'))
        return sm

    def on_start(self):
        Window.maximize()


if __name__ == '__main__':
    CheckersApp().run()