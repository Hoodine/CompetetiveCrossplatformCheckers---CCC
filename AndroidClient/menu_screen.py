from kivy.uix.screenmanager import Screen
from kivy.app import App


class MenuScreen(Screen):
    def connect_to_server(self):
        app = App.get_running_app()
        ip = self.ids.ip_input.text.strip()
        port_text = self.ids.port_input.text.strip()

        if not ip or not port_text:
            self.ids.status_label.text = 'Please enter IP and port'
            return

        try:
            port = int(port_text)
        except ValueError:
            self.ids.status_label.text = 'Invalid port'
            return

        self.ids.status_label.text = 'Connecting...'

        game_screen = app.root.get_screen('game')
        # Сбрасываем состояние перед подключением
        game_screen.reset_game()

        if game_screen.connect(ip, port):
            app.root.current = 'game'
        else:
            self.ids.status_label.text = 'Connection failed!'