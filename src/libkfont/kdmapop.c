/*
 * kdmapop.c - export getscrnmap(), loadscrnmap(),
 *                    loaduniscrnmap(), loadunimap()
 *
 * Hide the ioctl use in this file.
 */
#include "config.h"

#include <sys/ioctl.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "libcommon.h"

#include "contextP.h"

/*
 * Linux pre-0.96 defined GIO_SCRNMAP, PIO_SCRNMAP:
	typedef char scrnmap_t;
	#define E_TABSZ         256
	#define GIO_SCRNMAP     0x4B40
	#define PIO_SCRNMAP     0x4B41
 * and Linux 0.99.14y first implemented them.
 * Usage:
	scrnmap_t map[E_TABSZ];
	ioctl(fd,GIO_SCRNMAP,map);
	ioctl(fd,PIO_SCRNMAP,map);
 * to read or write the kernel translation table that is
 * applied to user application output before displaying.
 *
 * Before 1.3.1, the character set was completely undetermined,
 * and if the font was in an order different from the character
 * set in use, the user screen map was used to map application
 * codes to font indices. (To be more precise: there were four
 * translation tables, and this ioctl would get/set the fourth
 * table, while the other three tables are built-in and constant.)
 */
int
kfont_get_scrnmap(struct kfont_ctx *ctx, char *map)
{
	char buf[STACKBUF_LEN];

	if (ctx->consolefd < 0) {
		ERR(ctx, "console descriptor must be specified");
		return -1;
	}

	if (ioctl(ctx->consolefd, GIO_SCRNMAP, map)) {
		strerror_r(errno, buf, sizeof(buf));
		ERR(ctx, "ioctl(GIO_SCRNMAP): %s", buf);
		return -1;
	}
	return 0;
}

int
kfont_load_scrnmap(struct kfont_ctx *ctx, char *map)
{
	char buf[STACKBUF_LEN];

	if (ctx->consolefd < 0) {
		ERR(ctx, "console descriptor must be specified");
		return -1;
	}

	if (ioctl(ctx->consolefd, PIO_SCRNMAP, map)) {
		strerror_r(errno, buf, sizeof(buf));
		ERR(ctx, "ioctl(PIO_SCRNMAP): %s", buf);
		return -1;
	}
	return 0;
}

/*
 * Linux 1.3.1 introduces GIO_UNISCRNMAP, PIO_UNISCRNMAP:
	#define GIO_UNISCRNMAP  0x4B69
	#define PIO_UNISCRNMAP  0x4B6A
 * Usage:
	unsigned short umap[E_TABSZ];
	ioctl(fd,GIO_UNISCRNMAP,umap);
	ioctl(fd,PIO_UNISCRNMAP,umap);
 * to read or write the kernel translation table that is
 * applied to user application output before displaying.
 * (When the console is not in utf8 mode.)
 *
 * The idea is that the umap values are 16-bit unicode (ucs2)
 * values, and that the fonts will have an index giving the
 * unicode value for each glyph, so that the kernel can match up
 * application codes to font positions.
	#define UNI_DIRECT_BASE 0xF000
	#define UNI_DIRECT_MASK 0x01FF
 * For compatibility, and for fonts without table, the unicode
 * values 0xF000+n, 0 <= n <= 0x01FF, acts as direct font indices.
 * In the new scheme, the old PIO_SCRNMAP fills the kernel umap
 * table with such direct-to-font values.
 */

int
kfont_get_uniscrnmap(struct kfont_ctx *ctx, unsigned short *map)
{
	char buf[STACKBUF_LEN];

	if (ctx->consolefd < 0) {
		ERR(ctx, "console descriptor must be specified");
		return -1;
	}

	if (ioctl(ctx->consolefd, GIO_UNISCRNMAP, map)) {
		strerror_r(errno, buf, sizeof(buf));
		ERR(ctx, "ioctl(GIO_UNISCRNMAP): %s", buf);
		return -1;
	}
	return 0;
}

int
kfont_load_uniscrnmap(struct kfont_ctx *ctx, unsigned short *map)
{
	char buf[STACKBUF_LEN];

	if (ctx->consolefd < 0) {
		ERR(ctx, "console descriptor must be specified");
		return -1;
	}

	if (ioctl(ctx->consolefd, PIO_UNISCRNMAP, map)) {
		strerror_r(errno, buf, sizeof(buf));
		ERR(ctx, "ioctl(PIO_UNISCRNMAP): %s", buf);
		return -1;
	}
	return 0;
}

/*
 * Linux 1.1.63 introduces GIO_UNIMAP, PIO_UNIMAP, PIO_UNIMAPCLR:
	#define GIO_UNIMAP     0x4B66
	#define PIO_UNIMAP     0x4B67
	#define PIO_UNIMAPCLR  0x4B68
 * And Linux 1.1.92 implements them.
 * Usage:
	struct unimapinit {
        	unsigned short advised_hashsize;
		unsigned short advised_hashstep;
		unsigned short advised_hashlevel;
	} ui;
	ioctl(fd, PIO_UNIMAPCLR, &ui);
 * to clear the unimap table and advise about the kind of
 * hash setup appropriate to what one is going to load
 * (with 0 for "don't care").
	struct unipair {
	        unsigned short unicode;
		unsigned short fontpos;
	};
	struct unimapdesc {
        	unsigned short entry_ct;
		struct unipair *entries;
	} ud;
	ioctl(fd, PIO_UNIMAP, &ud);
	ioctl(fd, GIO_UNIMAP, &ud);
 * to add the indicated pairs to the kernel unimap table
 * or to read the kernel unimap table, where in the latter case
 * ud.entry_ct indicated the room available.
 *
 * In Linux 1.3.28 the hash table was replaced by a 3-level
 * paged table, so the contents of a struct unimapinit are
 * no longer meaningful.
 *
 * Linux 2.6.1 makes GIO_UNIMAP, PIO_UNIMAP, PIO_UNIMAPCLR per-vt
 * so that fd no longer is random.
 */
int
kfont_get_unimap(struct kfont_ctx *ctx, struct unimapdesc *ud0)
{
	char buf[STACKBUF_LEN];
	struct unimapdesc ud;
	unsigned int ct;

	if (ctx->consolefd < 0) {
		ERR(ctx, "console descriptor must be specified");
		return -1;
	}

	ud.entry_ct = 0;
	ud.entries = 0;

	if (ioctl(ctx->consolefd, GIO_UNIMAP, &ud)) {
		if (errno != ENOMEM || ud.entry_ct == 0) {
			strerror_r(errno, buf, sizeof(buf));
			ERR(ctx, "ioctl(GIO_UNIMAP,0): %s", buf);
			return -1;
		}

		ct = ud.entry_ct;

		ud.entries = (struct unipair *) malloc(ct * sizeof(struct unipair));

		if (ud.entries == NULL) {
			ERR(ctx, "out of memory");
			return -1;
		}

		if (ioctl(ctx->consolefd, GIO_UNIMAP, &ud)) {
			strerror_r(errno, buf, sizeof(buf));
			ERR(ctx, "ioctl(GIO_UNIMAP): %s", buf);
			return -1;
		}

		if (ct != ud.entry_ct)
			WARN(ctx, _("strange... ct changed from %d to %d\n"), ct, ud.entry_ct);
		/* someone could change the unimap between our
		   first and second ioctl, so the above errors
		   are not impossible */
	}
	*ud0 = ud;
	return 0;
}

int
kfont_load_unimap(struct kfont_ctx *ctx, struct unimapinit *ui, struct unimapdesc *ud)
{
	char buf[STACKBUF_LEN];
	struct unimapinit advice;

	if (ctx->consolefd < 0) {
		ERR(ctx, "console descriptor must be specified");
		return -1;
	}

	if (ui) {
		advice = *ui;
	} else {
		advice.advised_hashsize = 0;
		advice.advised_hashstep = 0;
		advice.advised_hashlevel = 0;
	}
again:
	if (ioctl(ctx->consolefd, PIO_UNIMAPCLR, &advice)) {
#ifdef ENOIOCTLCMD
		if (errno == ENOIOCTLCMD) {
			ERR(ctx, _("It seems this kernel is older than 1.1.92\n"
			           "No Unicode mapping table loaded.\n"));
		} else {
#endif
			strerror_r(errno, buf, sizeof(buf));
			ERR(ctx, "ioctl(PIO_UNIMAPCLR): %s", buf);
#ifdef ENOIOCTLCMD
		}
#endif
		return -1;
	}

	if (ud == NULL)
		return 0;

	if (ioctl(ctx->consolefd, PIO_UNIMAP, ud)) {
		if (errno == ENOMEM && advice.advised_hashlevel < 100) {
			advice.advised_hashlevel++;
			goto again;
		}
		strerror_r(errno, buf, sizeof(buf));
		ERR(ctx, "ioctl(PIO_UNIMAP): %s", buf);
		return -1;
	}

	return 0;
}