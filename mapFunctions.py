from countryData import *

countries = Countries()


def fill(surface, x, y, newColor):
    x = round(x)
    y = round(y)

    theStack = [(x, y)]
    oldColor = surface.get_at((x, y))

    if oldColor == newColor:
        return
    while len(theStack) > 0:
        x, y = theStack.pop()

        if surface.get_at((x, y)) != oldColor:
            continue
        surface.set_at((x, y), newColor)

        if x + 1 < surface.get_width():
            theStack.append((x + 1, y))
        if x - 1 > 0:
            theStack.append((x - 1, y))
        if y + 1 < surface.get_height():
            theStack.append((x, y + 1))
        if y - 1 > 0:
            theStack.append((x, y - 1))


def fillWithBorder(surface, referenceSurface, x, y, newColor):
    x, y = round(x), round(y)

    oldColor = surface.get_at((x, y))
    borderColor = (newColor[0] / 1.4, newColor[1] / 1.4, newColor[2] / 1.4)

    theStack = [(x, y)]
    border = []
    visited = set()

    while len(theStack) > 0:
        x, y = theStack.pop()

        if (x, y) in visited:
            continue

        visited.add((x, y))

        if surface.get_at((x, y)) != oldColor:
            color = referenceSurface.get_at((x, y))
            if countries.colorToCountry(color[:3]) == None:
                border.append((x, y))
            continue

        surface.set_at((x, y), newColor)

        theStack.append((x + 1, y))
        theStack.append((x - 1, y))
        theStack.append((x, y + 1))
        theStack.append((x, y - 1))

        extraPositions = (
            (x + 1, y + 1),
            (x - 1, y + 1),
            (x + 1, y - 1),
            (x - 1, y - 1),
        )
        for pos in extraPositions:
            r, g, b, a = referenceSurface.get_at(pos)
            if countries.colorToCountry((r, g, b)) == None:
                theStack.append(pos)

    for pixel in border:
        color = referenceSurface.get_at(pixel)
        if color[:3] == (0, 0, 0):
            surface.set_at(pixel, (0, 0, 0))
        elif color[:3] == (126, 142, 158):
            pass
        else:
            surface.set_at(pixel, borderColor)


def fillFixBorder(surface, x, y, newColor):
    x, y = round(x), round(y)

    oldColor = surface.get_at((x, y))
    borderColor = (newColor[0] / 1.4, newColor[1] / 1.4, newColor[2] / 1.4)

    theStack = [(x, y)]
    border = []
    visited = set()

    while len(theStack) > 0:
        x, y = theStack.pop()

        if (x, y) in visited:
            continue

        visited.add((x, y))

        if surface.get_at((x, y)) != oldColor:
            color = surface.get_at((x, y))
            if countries.colorToCountry((color[0], color[1], color[2])) == None:
                border.append((x, y))
            continue

        surface.set_at((x, y), newColor)

        theStack.append((x + 1, y))
        theStack.append((x - 1, y))
        theStack.append((x, y + 1))
        theStack.append((x, y - 1))

        extraPositions = (
            (x + 1, y + 1),
            (x - 1, y + 1),
            (x + 1, y - 1),
            (x - 1, y - 1),
        )
        for pos in extraPositions:
            r, g, b, a = surface.get_at(pos)
            if (r, g, b) == (0, 0, 0):
                theStack.append(pos)
            elif countries.colorToCountry((r, b, g)) == None:
                if not (r == g == b):
                    if (r, g, b) not in (newColor, oldColor, (126, 142, 158)):
                        theStack.append(pos)

    mapCopy = surface.copy()
    for pixel in border:
        x, y = pixel
        colors = []

        for i in range(3):
            for j in range(3):
                if not (0 <= x + i - 1 < surface.get_width()):
                    colors.append((0, 0, 0))
                    continue
                if not (0 <= y + j - 1 < surface.get_height()):
                    colors.append((0, 0, 0))
                    continue

                if surface.get_at((x + i - 1, y + j - 1)) not in colors:
                    if countries.colorToCountry(surface.get_at((x + i - 1, y + j - 1))[:3]) == None:
                        if surface.get_at((x + i - 1, y + j - 1))[:3] in ((105, 118, 132), (126, 142, 158)):
                            colors.append(surface.get_at((x + i - 1, y + j - 1)))
                    else:
                        colors.append(surface.get_at((x + i - 1, y + j - 1)))
                elif surface.get_at((x + i - 1, y + j - 1))[:3] in ((105, 118, 132), (126, 142, 158)):
                    colors.append(surface.get_at((x + i - 1, y + j - 1)))

        if len(colors) == 1:
            mapCopy.set_at((x, y), borderColor)
        else:
            mapCopy.set_at((x, y), (0, 0, 0))

    surface.blit(mapCopy, (0, 0))


def fixBorders(map, toChange=[(0, 0, 0)], toIgnore=[(105, 118, 132), (126, 142, 158)]):
    mapCopy = map.copy()

    toCheck = [(-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)]

    for y in range(map.get_height()):

        for x in range(map.get_width()):
            if map.get_at((x, y))[:3] not in toChange or map.get_at((x, y))[:3] in toIgnore:
                continue

            colors = []

            for i, j in toCheck:
                if not (0 <= x + i < map.get_width()):
                    colors.append((0, 0, 0))
                    continue
                if not (0 <= y + j < map.get_height()):
                    colors.append((0, 0, 0))
                    continue

                color = map.get_at((x + i, y + j))[:3]
                if color not in colors and color not in toChange:
                    colors.append(color)

            if len(colors) > 1:
                continue
            if len(colors) == 0:
                continue

            r, g, b = colors[0]
            mapCopy.set_at((x, y), (r // 1.4, g // 1.4, b // 1.4))

    return mapCopy
