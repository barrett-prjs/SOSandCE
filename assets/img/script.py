from PIL import Image
import numpy as np
import math

# =========================
# CONFIG
# =========================
IMAGE_PATH = "logo.png"
OUTPUT_PATH = "logo_recolored.png"
NUM_CLUSTERS = 6          # how many major color ranges to find
MAX_ITER = 25
ALPHA_THRESHOLD = 10      # ignore nearly transparent pixels


# =========================
# HELPERS
# =========================
def hex_to_rgb(value: str):
    value = value.strip().lstrip("#")
    if len(value) != 6:
        raise ValueError("Hex color must be 6 digits, like FFCC66")
    return tuple(int(value[i:i+2], 16) for i in (0, 2, 4))


def rgb_to_hex(rgb):
    return "#{:02X}{:02X}{:02X}".format(*rgb)


def brightness(rgb):
    r, g, b = rgb
    return 0.299 * r + 0.587 * g + 0.114 * b


def clamp(v, lo=0, hi=255):
    return max(lo, min(hi, int(round(v))))


def kmeans_colors(pixels, k=6, max_iter=25):
    """
    Simple k-means on RGB pixels.
    pixels: Nx3 float array
    """
    rng = np.random.default_rng(42)

    # pick initial centers from pixels
    idx = rng.choice(len(pixels), size=min(k, len(pixels)), replace=False)
    centers = pixels[idx].astype(np.float32)

    if len(centers) < k:
        # duplicate if too few pixels
        while len(centers) < k:
            centers = np.vstack([centers, centers[-1:]])

    for _ in range(max_iter):
        # distances to centers
        dists = np.sum((pixels[:, None, :] - centers[None, :, :]) ** 2, axis=2)
        labels = np.argmin(dists, axis=1)

        new_centers = []
        for i in range(k):
            members = pixels[labels == i]
            if len(members) == 0:
                new_centers.append(centers[i])
            else:
                new_centers.append(np.mean(members, axis=0))
        new_centers = np.array(new_centers, dtype=np.float32)

        if np.allclose(centers, new_centers, atol=1.0):
            break
        centers = new_centers

    # final labels
    dists = np.sum((pixels[:, None, :] - centers[None, :, :]) ** 2, axis=2)
    labels = np.argmin(dists, axis=1)
    return centers, labels


def remap_cluster_preserve_shading(image_rgba, valid_mask, labels, cluster_index, centers, new_base_rgb):
    """
    Recolors one cluster while preserving relative brightness/shading.
    """
    arr = image_rgba.copy().astype(np.float32)
    rgb = arr[..., :3]
    alpha = arr[..., 3]

    flat_rgb = rgb.reshape(-1, 3)
    flat_alpha = alpha.reshape(-1)
    flat_valid = valid_mask.reshape(-1)

    valid_indices = np.where(flat_valid)[0]
    target_valid_indices = valid_indices[labels == cluster_index]

    old_center = centers[cluster_index]
    old_center_bright = max(brightness(old_center), 1.0)
    new_base = np.array(new_base_rgb, dtype=np.float32)
    new_base_bright = max(brightness(new_base), 1.0)

    # recolor each pixel in chosen cluster
    for idx in target_valid_indices:
        old_pixel = flat_rgb[idx]
        old_bright = brightness(old_pixel)

        # preserve shading ratio relative to the old cluster center
        ratio = old_bright / old_center_bright

        # also preserve slight hue variation by blending with normalized old deviation
        recolored = new_base * ratio

        # clamp
        recolored = np.array([clamp(c) for c in recolored], dtype=np.float32)
        flat_rgb[idx] = recolored

    arr[..., :3] = flat_rgb.reshape(rgb.shape)
    arr[..., 3] = alpha
    return Image.fromarray(arr.astype(np.uint8), "RGBA")


# =========================
# MAIN
# =========================
def main():
    img = Image.open(IMAGE_PATH).convert("RGBA")
    arr = np.array(img)

    rgb = arr[..., :3]
    alpha = arr[..., 3]

    valid_mask = alpha > ALPHA_THRESHOLD
    valid_pixels = rgb[valid_mask]

    if len(valid_pixels) == 0:
        print("No visible pixels found.")
        return

    # Run clustering
    centers, labels = kmeans_colors(valid_pixels.astype(np.float32), k=NUM_CLUSTERS, max_iter=MAX_ITER)

    # Count cluster sizes
    counts = np.bincount(labels, minlength=NUM_CLUSTERS)
    order = np.argsort(counts)[::-1]

    print("\nDetected major color ranges:\n")
    for rank, cluster_idx in enumerate(order, start=1):
        center_rgb = tuple(clamp(c) for c in centers[cluster_idx])
        print(
            f"{rank}. Cluster {cluster_idx} | "
            f"Approx color: {center_rgb} {rgb_to_hex(center_rgb)} | "
            f"Pixels: {counts[cluster_idx]}"
        )

    # choose cluster
    while True:
        try:
            chosen_rank = int(input("\nEnter the RANK number to recolor (example: 1): ").strip())
            if 1 <= chosen_rank <= len(order):
                cluster_index = order[chosen_rank - 1]
                break
            print("Invalid rank.")
        except ValueError:
            print("Please enter a number.")

    # choose new base color
    while True:
        try:
            new_hex = input("Enter new base color in hex (example: C89B3C): ").strip()
            new_rgb = hex_to_rgb(new_hex)
            break
        except ValueError as e:
            print(e)

    result = remap_cluster_preserve_shading(arr, valid_mask, labels, cluster_index, centers, new_rgb)
    result.save(OUTPUT_PATH)

    chosen_center = tuple(clamp(c) for c in centers[cluster_index])
    print(f"\nDone.")
    print(f"Recolored cluster around {chosen_center} {rgb_to_hex(chosen_center)}")
    print(f"New base color: {new_rgb} {rgb_to_hex(new_rgb)}")
    print(f"Saved to: {OUTPUT_PATH}")


if __name__ == "__main__":
    main()