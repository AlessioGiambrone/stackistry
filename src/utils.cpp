/*
Stackistry - astronomical image stacking
Copyright (C) 2016, 2017 Filip Szczerek <ga.software@yahoo.com>

This file is part of Stackistry.

Stackistry is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Stackistry is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Stackistry.  If not, see <http://www.gnu.org/licenses/>.

File description:
    Utility functions implementation.
*/

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>

#include <cairomm/context.h>
#include <cairomm/surface.h>
#include <glibmm/i18n.h>
#include <glibmm/miscutils.h>
#include <gtkmm/cssprovider.h>

#include "config.h"
#include "utils.h"


namespace Utils
{

namespace Vars
{
    std::vector<OutputFormatDescr_t> outputFormatDescription;
    std::string appLaunchPath; ///< Value of argv[0]
}

Cairo::RefPtr<Cairo::ImageSurface> ConvertImgToSurface(const libskry::c_Image &img)
{
    libskry::c_Image imgBgra;

    const libskry::c_Image *pRgba;
    if (img.GetPixelFormat() != SKRY_PIX_BGRA8)
    {
        imgBgra = libskry::c_Image::ConvertPixelFormat(img, SKRY_PIX_BGRA8);
        if (!imgBgra)
            return Cairo::RefPtr<Cairo::ImageSurface>(nullptr);
        else
            pRgba = &imgBgra;
    }
    else
        pRgba = &img;

    Cairo::RefPtr<Cairo::ImageSurface> surface = Cairo::ImageSurface::create(Cairo::Format::FORMAT_RGB24, img.GetWidth(), img.GetHeight());
    for (unsigned row = 0; row < pRgba->GetHeight(); row++)
        std::memcpy(surface->get_data() + row * surface->get_stride(), pRgba->GetLine(row), surface->get_stride());

    return surface;
}

Cairo::Rectangle DrawAnchorPoint(const Cairo::RefPtr<Cairo::Context> &cr, int x, int y)
{
    cr->set_source_rgb(0.5, 0.2, 1);
    cr->move_to(x - Const::refPtDrawRadius, y);
    cr->line_to(x + Const::refPtDrawRadius, y);
    cr->move_to(x, y - Const::refPtDrawRadius);
    cr->line_to(x, y + Const::refPtDrawRadius);
    cr->stroke();


    return Cairo::Rectangle({
      (double)x - Utils::Const::refPtDrawRadius,
      (double)y - Utils::Const::refPtDrawRadius,
      2*Utils::Const::refPtDrawRadius + 2*cr->get_line_width(),
      2*Utils::Const::refPtDrawRadius + 2*cr->get_line_width() });
}

void EnumerateSupportedOutputFmts()
{
    size_t numFmts;
    const unsigned *supportedFmts = SKRY_get_supported_output_formats(&numFmts);

    for (size_t i = 0; i < numFmts; i++)
    {
        Vars::outputFormatDescription.push_back(Vars::OutputFormatDescr_t());
        Vars::OutputFormatDescr_t &descr = Vars::outputFormatDescription.back();
        descr.skryOutpFmt = (enum SKRY_output_format)supportedFmts[i];

        switch (supportedFmts[i])
        {
        case SKRY_BMP_8:
            descr.name = _("BMP 8-bit");
            descr.patterns = { "*.bmp" };
            descr.defaultExtension = ".bmp";
            break;

        case SKRY_TIFF_16:
            descr.name = _("TIFF 16-bit (uncompressed)");
            descr.patterns = { "*.tif", "*.tiff" };
            descr.defaultExtension = ".tif";
            break;

        case SKRY_PNG_8:
            descr.name = _("PNG 8-bit");
            descr.patterns = { "*.png" };
            descr.defaultExtension = ".png";
            break;
        }
    }
}

void RestorePosSize(const Gdk::Rectangle &posSize, Gtk::Window &wnd)
{
    //TODO: check if visible on any screen
    if (!Configuration::IsUndefined(posSize))
    {
        wnd.move(posSize.get_x(), posSize.get_y());
        wnd.resize(posSize.get_width(), posSize.get_height());
    }
}

void SavePosSize(const Gtk::Window &wnd, c_Property<Gdk::Rectangle> &destination)
{
    Gdk::Rectangle posSize;
    wnd.get_position(posSize.gobj()->x, posSize.gobj()->y);
    wnd.get_size(posSize.gobj()->width, posSize.gobj()->height);
    destination = posSize;
}

enum SKRY_pixel_format FindMatchingFormat(enum SKRY_output_format outputFmt, size_t numChannels)
{
    for (int i = SKRY_PIX_INVALID+1; i < SKRY_NUM_PIX_FORMATS; i++)
        if (i != SKRY_PIX_PAL8
            && NUM_CHANNELS[i] == numChannels
            && OUTPUT_FMT_BITS_PER_CHANNEL[outputFmt] == BITS_PER_CHANNEL[i])
        {
            return (SKRY_pixel_format)i;
        }

    return SKRY_PIX_INVALID;
}

const Vars::OutputFormatDescr_t &GetOutputFormatDescr(enum SKRY_output_format outpFmt)
{
    for (auto &descr: Vars::outputFormatDescription)
    {
        if (descr.skryOutpFmt == outpFmt)
            return descr;
    }

    assert(0);
}

Glib::RefPtr<Gdk::Pixbuf> LoadIconFromFile(const char *fileName, int width, int height)
{
    try
    {
        std::string fullPath = Glib::build_filename(
            Glib::path_get_dirname(Vars::appLaunchPath), "..", "icons", fileName);

        return Gdk::Pixbuf::create_from_file(fullPath, width, height);
    }
    catch (Glib::Exception &ex)
    {
        std::cerr << ex.what() << std::endl;
    }
    return Glib::RefPtr<Gdk::Pixbuf>(nullptr);
}

void SetAppLaunchPath(const char *appLaunchPath)
{
    Vars::appLaunchPath = appLaunchPath;
}

std::string GetErrorMsg(enum SKRY_result errorCode)
{
    switch (errorCode)
    {
    case SKRY_SUCCESS:                          return _("Success");
    case SKRY_INVALID_PARAMETERS:               return _("Invalid parameters");
    case SKRY_LAST_STEP:                        return _("Last step");
    case SKRY_NO_MORE_IMAGES:                   return _("No more images");
    case SKRY_NO_PALETTE:                       return _("No palette");
    case SKRY_CANNOT_OPEN_FILE:                 return _("Cannot open file");
    case SKRY_BMP_MALFORMED_FILE:               return _("Malformed BMP file");
    case SKRY_UNSUPPORTED_BMP_FILE:             return _("Unsupported BMP file");
    case SKRY_UNSUPPORTED_FILE_FORMAT:          return _("Unsupported file format");
    case SKRY_OUT_OF_MEMORY:                    return _("Out of memory");
    case SKRY_CANNOT_CREATE_FILE:               return _("Cannot create file");
    case SKRY_TIFF_INCOMPLETE_HEADER:           return _("Incomplete TIFF header");
    case SKRY_TIFF_UNKNOWN_VERSION:             return _("Unknown TIFF version");
    case SKRY_TIFF_NUM_DIR_ENTR_TAG_INCOMPLETE: return _("Incomplete TIFF tag: number of directory entries");
    case SKRY_TIFF_INCOMPLETE_FIELD:            return _("Incomplete TIFF field");
    case SKRY_TIFF_DIFF_CHANNEL_BIT_DEPTHS:     return _("Channels have different bit depths");
    case SKRY_TIFF_COMPRESSED:                  return _("TIFF compression is not supported");
    case SKRY_TIFF_UNSUPPORTED_PLANAR_CONFIG:   return _("Unsupported TIFF planar configuration");
    case SKRY_UNSUPPORTED_PIXEL_FORMAT:         return _("Unsupported pixel format");
    case SKRY_TIFF_INCOMPLETE_PIXEL_DATA:       return _("Incomplete TIFF pixel data");
    case SKRY_AVI_MALFORMED_FILE:               return _("Malformed AVI file");
    case SKRY_AVI_UNSUPPORTED_FORMAT:           return _("Unsupported AVI video format");
    case SKRY_INVALID_IMG_DIMENSIONS:           return _("Invalid image dimensions");
    case SKRY_SER_MALFORMED_FILE:               return _("Malformed SER file");
    case SKRY_SER_UNSUPPORTED_FORMAT:           return _("Unsupported SER format");

    case SKRY_LIBAV_NO_VID_STREAM:              return _("Video stream not found");
    case SKRY_LIBAV_UNSUPPORTED_FORMAT:         return _("Unsupported format");
    case SKRY_LIBAV_DECODING_ERROR:             return _("Decoding error");
    case SKRY_LIBAV_INTERNAL_ERROR:             return _("Internal libav error");

    default:                                    return _("Unknown error");
    }
}

Cairo::Filter GetFilter(Const::InterpolationMethod interpolationMethod)
{
    /// Order of elements must reflect Utils::InterpolationMethod
    static const Cairo::Filter cairoFilter[] =
    {
        Cairo::Filter::FILTER_FAST,
        Cairo::Filter::FILTER_GOOD,
        Cairo::Filter::FILTER_BEST
    };

    return cairoFilter[(int)interpolationMethod];
}

void SetBackgroundColor(Gtk::Widget &w, const Gdk::RGBA &color)
{
    Glib::RefPtr<Gtk::CssProvider> css = Gtk::CssProvider::create();
    css->load_from_data(Glib::ustring::compose(".stackistry_custom_bkgrnd { background-color: %1; }",
                                               color.to_string()));

    auto styleCtx = w.get_style_context();
    styleCtx->add_provider(css, GTK_STYLE_PROVIDER_PRIORITY_USER);
    styleCtx->add_class("stackistry_custom_bkgrnd");
    styleCtx->context_save();
}

void SetColor(const Cairo::RefPtr<Cairo::Context> &cr, const GdkRGBA &color)
{
    cr->set_source_rgba(color.red, color.green, color.blue, color.alpha);
}

}
