#include "widgets/OverlayBackdropCapture.h"

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <QElapsedTimer>
#include <QPainter>
#include <QTransform>
#include <algorithm>
#include <cstring>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {
struct Output {
    QRect bounds;
    DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_IDENTITY;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIOutput1> output;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGIOutputDuplication> duplication;
    ComPtr<ID3D11Texture2D> frame;
    ComPtr<ID3D11Texture2D> staging;
    QElapsedTimer retry;

    void release() {
        staging.Reset();
        frame.Reset();
        duplication.Reset();
        context.Reset();
        device.Reset();
    }

    bool initialize() {
        if (duplication) return true;
        if (retry.isValid() && retry.elapsed() < 1000) return false;
        retry.restart();
        release();
        if (FAILED(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
                device.GetAddressOf(), nullptr, context.GetAddressOf()))
            || FAILED(output->DuplicateOutput(device.Get(), duplication.GetAddressOf()))) {
            release();
            return false;
        }
        return true;
    }

    bool update() {
        if (!initialize()) return false;
        DXGI_OUTDUPL_FRAME_INFO info{};
        ComPtr<IDXGIResource> resource;
        const HRESULT result = duplication->AcquireNextFrame(0, &info, resource.GetAddressOf());
        if (result == DXGI_ERROR_WAIT_TIMEOUT) return frame != nullptr;
        if (FAILED(result)) {
            release();
            return false;
        }
        ComPtr<ID3D11Texture2D> acquired;
        bool valid = SUCCEEDED(resource.As(&acquired));
        if (valid && !frame) {
            D3D11_TEXTURE2D_DESC description{};
            acquired->GetDesc(&description);
            description.Usage = D3D11_USAGE_DEFAULT;
            description.BindFlags = 0;
            description.CPUAccessFlags = 0;
            description.MiscFlags = 0;
            valid = SUCCEEDED(device->CreateTexture2D(&description, nullptr, frame.GetAddressOf()));
        }
        if (valid) context->CopyResource(frame.Get(), acquired.Get());
        const HRESULT released = duplication->ReleaseFrame();
        if (!valid || FAILED(released)) {
            release();
            return false;
        }
        return true;
    }

    QImage crop(const QRect& area) {
        if (!update()) return {};
        const QRect local = area.translated(-bounds.topLeft());
        D3D11_TEXTURE2D_DESC description{};
        frame->GetDesc(&description);
        QRect raw = local;
        int angle = 0;
        if (rotation == DXGI_MODE_ROTATION_ROTATE90) {
            raw = QRect(local.y(), static_cast<int>(description.Height) - local.x() - local.width(),
                local.height(), local.width());
            angle = 90;
        } else if (rotation == DXGI_MODE_ROTATION_ROTATE180) {
            raw.moveTopLeft(QPoint(static_cast<int>(description.Width) - local.x() - local.width(),
                static_cast<int>(description.Height) - local.y() - local.height()));
            angle = 180;
        } else if (rotation == DXGI_MODE_ROTATION_ROTATE270) {
            raw = QRect(static_cast<int>(description.Width) - local.y() - local.height(), local.x(),
                local.height(), local.width());
            angle = 270;
        }
        if (!QRect(0, 0, description.Width, description.Height).contains(raw)) return {};
        D3D11_TEXTURE2D_DESC previous{};
        if (staging) staging->GetDesc(&previous);
        if (!staging || previous.Width != static_cast<UINT>(raw.width())
            || previous.Height != static_cast<UINT>(raw.height())) {
            staging.Reset();
            description.Width = raw.width();
            description.Height = raw.height();
            description.Usage = D3D11_USAGE_STAGING;
            description.BindFlags = 0;
            description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            description.MiscFlags = 0;
            if (FAILED(device->CreateTexture2D(&description, nullptr, staging.GetAddressOf()))) return {};
        }
        const D3D11_BOX box{static_cast<UINT>(raw.left()), static_cast<UINT>(raw.top()), 0,
            static_cast<UINT>(raw.x() + raw.width()), static_cast<UINT>(raw.y() + raw.height()), 1};
        context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, frame.Get(), 0, &box);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
            release();
            return {};
        }
        QImage image(raw.size(), QImage::Format_RGB32);
        for (int y = 0; y < image.height(); ++y) {
            const auto* row = static_cast<const unsigned char*>(mapped.pData) + y * mapped.RowPitch;
            std::memcpy(image.scanLine(y), row, static_cast<size_t>(image.width()) * 4);
            auto* pixels = reinterpret_cast<QRgb*>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x) pixels[x] |= 0xff000000;
        }
        context->Unmap(staging.Get(), 0);
        if (angle) image = image.transformed(QTransform().rotate(angle));
        return image;
    }
};

void extendScreenEdges(QImage& image) {
    int firstRow = -1;
    int lastRow = -1;
    for (int y = 0; y < image.height(); ++y) {
        auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
        int first = 0;
        while (first < image.width() && qAlpha(row[first]) == 0) ++first;
        if (first == image.width()) continue;
        if (firstRow < 0) firstRow = y;
        lastRow = y;
        std::fill(row, row + first, row[first]);
        for (int x = first + 1; x < image.width(); ++x)
            if (qAlpha(row[x]) == 0) row[x] = row[x - 1];
    }
    if (firstRow < 0) return;
    for (int y = 0; y < firstRow; ++y)
        std::memcpy(image.scanLine(y), image.constScanLine(firstRow), image.bytesPerLine());
    for (int y = lastRow + 1; y < image.height(); ++y)
        std::memcpy(image.scanLine(y), image.constScanLine(lastRow), image.bytesPerLine());
}
}

struct OverlayBackdropCapture::State {
    std::vector<Output> outputs;
    ComPtr<IDXGIFactory1> factory;
    QElapsedTimer retry;

    bool initialize() {
        if (factory && !outputs.empty() && factory->IsCurrent()) return true;
        if (retry.isValid() && retry.elapsed() < 1000) return false;
        retry.restart();
        outputs.clear();
        factory.Reset();
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf())))) return false;
        for (UINT adapterIndex = 0;; ++adapterIndex) {
            ComPtr<IDXGIAdapter1> adapter;
            if (factory->EnumAdapters1(adapterIndex, adapter.GetAddressOf()) != S_OK) break;
            for (UINT outputIndex = 0;; ++outputIndex) {
                ComPtr<IDXGIOutput> display;
                if (adapter->EnumOutputs(outputIndex, display.GetAddressOf()) != S_OK) break;
                DXGI_OUTPUT_DESC description{};
                if (FAILED(display->GetDesc(&description)) || !description.AttachedToDesktop) continue;
                Output output;
                if (FAILED(display.As(&output.output))) continue;
                output.adapter = adapter;
                output.rotation = description.Rotation;
                const RECT& rect = description.DesktopCoordinates;
                output.bounds = QRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
                outputs.push_back(std::move(output));
            }
        }
        return !outputs.empty();
    }
};

OverlayBackdropCapture::OverlayBackdropCapture() : state_(std::make_unique<State>()) {}
OverlayBackdropCapture::~OverlayBackdropCapture() = default;

void OverlayBackdropCapture::reset() {
    state_ = std::make_unique<State>();
}

QImage OverlayBackdropCapture::grab(const QRect& physicalRect) {
    if (physicalRect.isEmpty() || !state_->initialize()) return {};
    QImage image(physicalRect.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    bool captured = false;
    QPainter painter(&image);
    for (auto& output : state_->outputs) {
        const QRect area = physicalRect.intersected(output.bounds);
        if (area.isEmpty()) {
            output.release();
            output.retry.invalidate();
            continue;
        }
        const QImage sample = output.crop(area);
        if (sample.isNull()) return {};
        painter.drawImage(area.topLeft() - physicalRect.topLeft(), sample);
        captured = true;
    }
    painter.end();
    if (!captured) return {};
    extendScreenEdges(image);
    return image;
}
