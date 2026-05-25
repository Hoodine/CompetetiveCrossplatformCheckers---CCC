from kivy.uix.screenmanager import Screen
from kivy.app import App
from kivy.clock import Clock
from kivy.graphics import Color, Rectangle, Ellipse, Line
from kivy.metrics import dp
from kivy.core.text import Label as CoreLabel
from network import NetworkClient


class GameScreen(Screen):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.client = NetworkClient()
        self.board = [[0] * 8 for _ in range(8)]
        self.my_color = None
        self.current_turn = None
        self.selected_piece = None
        self.possible_moves = []
        self.capture_forced = None
        self.cell_size = dp(40)
        self.winner = None
        self.board_pos = (0, 0, 0, 0)
        self.game_over_reason = ""
        self.game_over_shown = False
        self.waiting_for_disconnect = False

        Clock.schedule_interval(self.update, 0.05)

    def on_size(self, *args):
        Clock.schedule_once(lambda dt: self._update_board_position(), 0.1)

    def connect(self, ip, port):
        if self.client.connect(ip, port):
            self.ids.game_status.text = 'Connected, waiting...'
            self.ids.message_label.text = ''
            self.game_over_shown = False
            self.winner = None
            self.game_over_reason = ""
            self.waiting_for_disconnect = False
            return True
        else:
            self.ids.game_status.text = 'Connection failed!'
            return False

    def disconnect(self):
        """Отключение и возврат в меню"""
        self.waiting_for_disconnect = False
        self.client.disconnect()
        self.reset_game()
        app = App.get_running_app()
        # Переключаемся на меню через 0.1 секунды, чтобы не мешать отрисовке
        Clock.schedule_once(lambda dt: setattr(app.root, 'current', 'menu'), 0.1)

    def reset_game(self):
        """Сброс состояния игры"""
        self.selected_piece = None
        self.possible_moves = []
        self.capture_forced = None
        self.board = [[0] * 8 for _ in range(8)]
        self.winner = None
        self.my_color = None
        self.current_turn = None
        self.game_over_reason = ""
        self.game_over_shown = False
        self.waiting_for_disconnect = False

    def update(self, dt):
        # Если ждем отключения, не обрабатываем новые сообщения
        if self.waiting_for_disconnect:
            return

        msg = self.client.poll_message()
        if msg:
            self._handle_message(msg)

        # Постоянно обновляем позицию доски для точных кликов
        self._update_board_position()

    def _update_board_position(self, dt=None):
        """Обновляет позицию доски для точных кликов"""
        board_area = self.ids.board_area
        if not board_area:
            return

        x = board_area.x
        y = board_area.y

        parent = board_area.parent
        while parent and parent != self:
            x += parent.x
            y += parent.y
            parent = parent.parent

        w = board_area.width
        h = board_area.height

        if w > 0 and h > 0:
            board_size = min(w, h) * 0.95
            board_left = x + (w - board_size) / 2
            board_top = y + (h - board_size) / 2
            self.board_pos = (board_left, board_top, board_size, board_size)
            self.cell_size = board_size / 8

    def _handle_message(self, msg):
        msg_type = msg.get('type', '')
        print(f"Received: {msg_type}")

        if msg_type == 'game_start':
            self.my_color = msg['color']
            self.winner = None
            self.game_over_reason = ""
            self.game_over_shown = False
            self.waiting_for_disconnect = False
            self.ids.game_status.text = f'You are {self.my_color}'
            self.ids.message_label.text = 'Waiting for board...'
            self.selected_piece = None
            self.possible_moves = []
            self.capture_forced = None
            self._draw_board()

        elif msg_type == 'board_state':
            if self.winner or self.game_over_shown:
                return

            self.board = [[int(cell) for cell in row] for row in msg['board']]
            self.current_turn = msg.get('turn', '')

            # Проверяем победу по отсутствию фигур
            white_count = 0
            black_count = 0
            for row in self.board:
                for cell in row:
                    if cell in (1, 3):
                        white_count += 1
                    elif cell in (2, 4):
                        black_count += 1

            if white_count == 0 and not self.winner and not self.game_over_shown:
                self._show_game_over('black', 'All white pieces captured')
                return
            elif black_count == 0 and not self.winner and not self.game_over_shown:
                self._show_game_over('white', 'All black pieces captured')
                return

            if self.my_color == self.current_turn:
                turn_text = f'YOUR TURN ({self.my_color})'
            else:
                turn_text = f'Waiting for opponent ({self.current_turn})'
            self.ids.game_status.text = turn_text
            self.ids.message_label.text = ''

            if 'must_capture_from' in msg:
                pos = msg['must_capture_from']
                col = ord(pos[0]) - ord('a')
                row = 8 - int(pos[1])
                self.capture_forced = (row, col)
                self.selected_piece = (row, col)
                self.ids.message_label.text = 'MUST CAPTURE!'
                self.client.send_message({'type': 'get_moves', 'from': pos})
            else:
                self.capture_forced = None
                if self.my_color != self.current_turn:
                    self.selected_piece = None
                    self.possible_moves = []

            self._draw_board()

        elif msg_type == 'possible_moves':
            self.possible_moves = msg.get('moves', [])
            self._draw_board()

        elif msg_type == 'error':
            self.ids.message_label.text = msg.get('message', 'Error')
            self.selected_piece = None
            self.possible_moves = []
            self._draw_board()

        elif msg_type == 'game_over':
            winner = msg.get('winner', '')
            reason = msg.get('reason', '')
            print(f"GAME OVER message received! Winner: {winner}, Reason: {reason}")

            if not self.game_over_shown:
                self._show_game_over(winner, reason)

    def _show_game_over(self, winner, reason):
        """Отображает сообщение о конце игры"""
        if self.game_over_shown:
            return

        self.winner = winner
        self.game_over_reason = reason
        self.game_over_shown = True
        self.waiting_for_disconnect = True  # Останавливаем обработку новых сообщений

        reason_text = {
            'resignation': 'Opponent resigned',
            'all_pieces_captured': 'All pieces captured',
            'no_moves': 'No moves available',
            'opponent_disconnected': 'Opponent disconnected'
        }.get(reason, reason)

        if winner == self.my_color:
            self.ids.game_status.text = ' VICTORY! '
            self.ids.message_label.text = reason_text
            print(f" GAME OVER - YOU WIN! ")
        else:
            self.ids.game_status.text = ' DEFEAT! '
            self.ids.message_label.text = reason_text
            print(f" GAME OVER - YOU LOSE! ")

        self.selected_piece = None
        self.possible_moves = []

        # НЕМЕДЛЕННО перерисовываем доску с оверлеем (2 раза для надежности)
        self._draw_board()
        Clock.schedule_once(lambda dt: self._draw_board(), 0.05)
        Clock.schedule_once(lambda dt: self._draw_board(), 0.1)

    def on_touch_down(self, touch):
        import time
        current_time = time.time()

        if not hasattr(self, '_last_touch_time'):
            self._last_touch_time = 0
        if current_time - self._last_touch_time < 0.15:
            return super().on_touch_down(touch)
        self._last_touch_time = current_time

        # Если игра окончена, показываем сообщение и не обрабатываем клики по доске
        if self.winner or self.game_over_shown:
            # Но позволяем кликнуть по кнопке Disconnect
            if hasattr(self.ids, 'disconnect_button') and self.ids.disconnect_button.collide_point(*touch.pos):
                self.disconnect()
            return super().on_touch_down(touch)

        if not self.board or self.my_color is None:
            return super().on_touch_down(touch)

        if self.current_turn != self.my_color:
            self.ids.message_label.text = "It's not your turn"
            return super().on_touch_down(touch)

        self._update_board_position()

        board_left, board_top, board_width, board_height = self.board_pos

        if board_width == 0:
            return super().on_touch_down(touch)

        x, y = touch.pos

        if x < board_left or x > board_left + board_width or y < board_top or y > board_top + board_height:
            if not self.capture_forced:
                self.selected_piece = None
                self.possible_moves = []
                self._draw_board()
            return super().on_touch_down(touch)

        cell_w = board_width / 8
        cell_h = board_height / 8

        col = int((x - board_left) / cell_w)
        row = int((y - board_top) / cell_h)
        row = 7 - row

        row = max(0, min(7, row))
        col = max(0, min(7, col))

        target = chr(ord('a') + col) + str(8 - row)

        if self.selected_piece and target in self.possible_moves:
            from_pos = chr(ord('a') + self.selected_piece[1]) + str(8 - self.selected_piece[0])
            self.client.send_message({
                'type': 'move',
                'from': from_pos,
                'to': target
            })
            self.selected_piece = None
            self.possible_moves = []
            self._draw_board()
            return super().on_touch_down(touch)

        if 0 <= row < 8 and 0 <= col < 8:
            piece = self.board[row][col]
            is_white = self.my_color == 'white'
            is_my_piece = (piece == 1 or piece == 3) if is_white else (piece == 2 or piece == 4)

            if is_my_piece:
                if self.capture_forced and (row, col) != self.capture_forced:
                    self.ids.message_label.text = 'You must capture!'
                    return super().on_touch_down(touch)

                self.selected_piece = (row, col)
                pos = chr(ord('a') + col) + str(8 - row)
                self.client.send_message({'type': 'get_moves', 'from': pos})
                self._draw_board()
            else:
                if not self.capture_forced:
                    self.selected_piece = None
                    self.possible_moves = []
                    self._draw_board()

        return super().on_touch_down(touch)

    def _draw_board(self):
        board_area = self.ids.board_area
        if not board_area:
            return

        board_area.canvas.clear()

        x = board_area.x
        y = board_area.y
        parent = board_area.parent
        while parent and parent != self:
            x += parent.x
            y += parent.y
            parent = parent.parent

        bw = board_area.width
        bh = board_area.height

        with board_area.canvas:
            Color(0.15, 0.15, 0.15)
            Rectangle(pos=(x, y), size=(bw, bh))

            board_size = min(bw, bh) * 0.95
            cell_size = board_size / 8
            board_left = x + (bw - board_size) / 2
            board_top = y + (bh - board_size) / 2

            self.board_pos = (board_left, board_top, board_size, board_size)

            # Рисуем клетки
            for r in range(8):
                for c in range(8):
                    cell_x = board_left + c * cell_size
                    cell_y = board_top + (7 - r) * cell_size

                    if (r + c) % 2 == 0:
                        Color(0.94, 0.85, 0.71)
                    else:
                        Color(0.71, 0.53, 0.39)

                    Rectangle(pos=(cell_x, cell_y), size=(cell_size, cell_size))

                    if len(self.board) > r and len(self.board[r]) > c:
                        piece = self.board[r][c]
                        if piece != 0:
                            cx = cell_x + cell_size / 2
                            cy = cell_y + cell_size / 2
                            radius = cell_size * 0.38

                            if piece == 1:
                                Color(1, 1, 1)
                            elif piece == 2:
                                Color(0.1, 0.1, 0.1)
                            elif piece == 3:
                                Color(1, 1, 1)
                            elif piece == 4:
                                Color(0.1, 0.1, 0.1)

                            Ellipse(pos=(cx - radius, cy - radius), size=(radius * 2, radius * 2))

                            if piece in (3, 4):
                                Color(1, 0.84, 0)
                                Line(circle=(cx, cy, radius + dp(2)), width=dp(2))

            # Выделение выбранной фигуры
            if self.selected_piece and not self.winner:
                r, c = self.selected_piece
                cell_x = board_left + c * cell_size
                cell_y = board_top + (7 - r) * cell_size

                Color(1, 1, 0, 0.4)
                Rectangle(pos=(cell_x, cell_y), size=(cell_size, cell_size))

                if self.capture_forced and self.capture_forced == (r, c):
                    Color(1, 0, 0, 0.8)
                    Line(rectangle=(cell_x, cell_y, cell_size, cell_size), width=dp(3))

            # Возможные ходы
            if self.possible_moves and not self.winner:
                for move in self.possible_moves:
                    mc = ord(move[0]) - ord('a')
                    mr = 8 - int(move[1])
                    mx = board_left + mc * cell_size + cell_size / 2
                    my = board_top + (7 - mr) * cell_size + cell_size / 2
                    radius = cell_size * 0.15

                    Color(0, 0.8, 0, 0.7)
                    Ellipse(pos=(mx - radius, my - radius), size=(radius * 2, radius * 2))
                    Color(0, 1, 0, 0.9)
                    Line(circle=(mx, my, radius), width=dp(1))

            # ОВЕРЛЕЙ ПРИ КОНЦЕ ИГРЫ
            if self.winner:
                # Полупрозрачный фон
                Color(0, 0, 0, 0.92)
                Rectangle(pos=(x, y), size=(bw, bh))

                # Текст
                if self.winner == self.my_color:
                    text = "VICTORY!"
                    subtext = "You win!"
                    text_color = (0.2, 0.9, 0.2, 1)
                else:
                    text = "DEFEAT!"
                    subtext = "You lose!"
                    text_color = (1, 0.2, 0.2, 1)

                # Большой текст
                font_size_main = min(bw, bh) * 0.12
                label = CoreLabel(text=text, font_size=font_size_main,
                                  color=text_color, bold=True)
                label.refresh()
                texture = label.texture

                tex_x = x + (bw - texture.width) / 2
                tex_y = y + (bh - texture.height) / 2 + min(bw, bh) * 0.05
                Color(1, 1, 1, 1)
                Rectangle(texture=texture, pos=(tex_x, tex_y), size=texture.size)

                # Подзаголовок
                font_size_sub = min(bw, bh) * 0.06
                sub_label = CoreLabel(text=subtext, font_size=font_size_sub,
                                      color=(1, 1, 0.8, 1))
                sub_label.refresh()
                sub_texture = sub_label.texture

                sub_x = x + (bw - sub_texture.width) / 2
                sub_y = tex_y - sub_texture.height - dp(10)
                if sub_y > y + dp(10):
                    Color(1, 1, 1, 1)
                    Rectangle(texture=sub_texture, pos=(sub_x, sub_y), size=sub_texture.size)

                # Причина
                reason_display = {
                    'resignation': 'Opponent resigned',
                    'all_pieces_captured': 'All pieces captured',
                    'no_moves': 'No moves available',
                    'opponent_disconnected': 'Opponent disconnected',
                    'All white pieces captured': 'White pieces captured',
                    'All black pieces captured': 'Black pieces captured'
                }.get(self.game_over_reason, self.game_over_reason)

                font_size_reason = min(bw, bh) * 0.035
                reason_label = CoreLabel(text=reason_display, font_size=font_size_reason,
                                         color=(0.9, 0.9, 0.9, 1))
                reason_label.refresh()
                reason_texture = reason_label.texture

                reason_x = x + (bw - reason_texture.width) / 2
                reason_y = sub_y - reason_texture.height - dp(8)
                if reason_y > y + dp(10):
                    Color(1, 1, 1, 1)
                    Rectangle(texture=reason_texture, pos=(reason_x, reason_y),
                              size=reason_texture.size)

                # Кнопка Disconnect
                btn_text = "Tap here to exit"
                font_size_btn = min(bw, bh) * 0.04
                btn_label = CoreLabel(text=btn_text, font_size=font_size_btn,
                                      color=(0.8, 0.8, 0.8, 1))
                btn_label.refresh()
                btn_texture = btn_label.texture

                btn_x = x + (bw - btn_texture.width) / 2
                btn_y = y + dp(20)
                Color(0.3, 0.3, 0.3, 0.8)
                Rectangle(pos=(btn_x - dp(10), btn_y - dp(5)),
                          size=(btn_texture.width + dp(20), btn_texture.height + dp(10)))
                Color(1, 1, 1, 1)
                Rectangle(texture=btn_texture, pos=(btn_x, btn_y), size=btn_texture.size)