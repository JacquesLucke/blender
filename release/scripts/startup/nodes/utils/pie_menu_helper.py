class PieMenuHelper:
    def draw(self, context):
        pie = self.layout.menu_pie()
        self.draw_left(pie)
        self.draw_right(pie)
        self.draw_bottom(pie)
        self.draw_top(pie)
        self.draw_top_left(pie)
        self.draw_top_right(pie)
        self.draw_bottom_left(pie)
        self.draw_bottom_right(pie)

    def draw_left(self, layout):
        self.empty(layout)

    def draw_right(self, layout):
        self.empty(layout)

    def draw_bottom(self, layout):
        self.empty(layout)

    def draw_top(self, layout):
        self.empty(layout)

    def draw_top_left(self, layout):
        self.empty(layout)

    def draw_top_right(self, layout):
        self.empty(layout)

    def draw_bottom_left(self, layout):
        self.empty(layout)

    def draw_bottom_right(self, layout):
        self.empty(layout)

    def empty(self, layout, text=""):
        layout.row().label(text=text)