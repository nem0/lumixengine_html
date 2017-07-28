#include "editor/render_interface.h"
#include "editor/studio_app.h"
#include "editor/world_editor.h"
#include "engine/crc32.h"
#include "engine/fs/os_file.h"
#include "engine/hash_map.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "litehtml.h"
#include "stb/stb_image.h"
#include <Windows.h>
#include <cmath>
#include <cstdlib>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")


using namespace Lumix;


struct HTMLDocumentContainer : litehtml::document_container
{
	HTMLDocumentContainer(StudioApp& app)
		: m_app(app)
		, m_images(app.getWorldEditor()->getAllocator())
	{
	}

	static bool download(const char* host, const char* path, Array<u8>* out)
	{
		char url[1024];
		if (startsWith(path, "http://") || startsWith(path, "https://"))
		{
			strcpy_s(url, path);
		}
		else
		{
			strcpy_s(url, "http://");
			strcat_s(url, host);
			strcat_s(url, "/");
			strcat_s(url, path);
		}

		HINTERNET net = InternetOpen("LumixEngine", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
		HINTERNET net2 = InternetOpenUrl(net, url, "", 0, INTERNET_FLAG_EXISTING_CONNECT, 0);
		// todo close resources
		char buff[4096];
		DWORD read;
		while (InternetReadFile(net2, buff, 4069, &read)) {
			if (read == 0) break;

			out->resize(out->size() + read);
			memcpy(&(*out)[out->size() - read], buff, read);
		}
		out->resize(out->size() + 1);
		(*out)[out->size() - 1] = '\0';

		return true;
	}


	litehtml::uint_ptr create_font(const litehtml::tchar_t* faceName,
		int size,
		int weight,
		litehtml::font_style italic,
		unsigned int decoration,
		litehtml::font_metrics* fm) override
	{
		RenderInterface* ri = m_app.getWorldEditor()->getRenderInterface();
		ImFont* font = ri->addFont("bin/veramono.ttf", size);

		fm->height = font->Ascent - font->Descent;
		fm->ascent = font->Ascent;
		fm->descent = font->Descent;
		return (litehtml::uint_ptr)font;
	}


	void delete_font(litehtml::uint_ptr hFont) override {}


	int text_width(const litehtml::tchar_t* text, litehtml::uint_ptr hFont) override
	{
		if (!hFont) return 50;
		
		ImGui::PushFont((ImFont*)hFont);
		ImVec2 size = ImGui::CalcTextSize(text);
		ImGui::PopFont();

		return size.x;
	}


	void draw_text(litehtml::uint_ptr hdc,
		const litehtml::tchar_t* text,
		litehtml::uint_ptr hFont,
		litehtml::web_color color,
		const litehtml::position& pos) override
	{
		ImGuiWindow* win = ImGui::GetCurrentWindow();
		ImVec2 imgui_pos = {win->Pos.x + (float)pos.x, win->Pos.y + (float)pos.y};
		ImColor col(color.red, color.green, color.blue, color.alpha);
		ImFont* font = (ImFont*)hFont;
		win->DrawList->AddText(font, font->FontSize, imgui_pos, col, text);
	}


	int pt_to_px(int pt) override { /*TODO*/ return pt; }
	int get_default_font_size() const override { return 16; }
	const litehtml::tchar_t* get_default_font_name() const override { return _t("Times New Roman"); }
	void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) override {}
	
	
	void load_image(const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, bool redraw_on_ready) override
	{
		RenderInterface* ri = m_app.getWorldEditor()->getRenderInterface();
		IAllocator& allocator = m_app.getWorldEditor()->getAllocator();
		Array<u8> data(allocator);
		download(m_host, src, &data);
		int channels;
		Image img;
		stbi_uc* pixels = stbi_load_from_memory(&data[0], data.size(), &img.w, &img.h, &channels, 4);
		if (!pixels) return;
		img.texture = ri->createTexture(src, pixels, img.w, img.h);
		m_images.insert(crc32(src), img);
	}


	void get_image_size(const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, litehtml::size& sz) override 
	{
		auto iter = m_images.find(crc32(src));
		if (iter.isValid())
		{
			sz.width = iter.value().w;
			sz.height = iter.value().h;
		}
		else
		{
			sz.width = sz.height = 100;
		}
	}


	void draw_background(litehtml::uint_ptr hdc, const litehtml::background_paint& bg) override {
		
		ImGuiWindow* win = ImGui::GetCurrentWindow();
		ImVec2 a(win->Pos.x + bg.clip_box.left(), win->Pos.y + bg.clip_box.top());
		ImVec2 b(win->Pos.x + bg.clip_box.right(), win->Pos.y + bg.clip_box.bottom());
		if (bg.image.empty())
		{
			ImColor col(bg.color.red, bg.color.green, bg.color.blue, bg.color.alpha);
			win->DrawList->AddRectFilled(a, b, col);
			return;
		}

		auto iter = m_images.find(crc32(bg.image.c_str()));
		if (!iter.isValid()) return;

		auto img = iter.value();

		switch (bg.repeat)
		{
			case litehtml::background_repeat_no_repeat: win->DrawList->AddImage(img.texture, a, b); break;
			case litehtml::background_repeat_repeat_x:
			{
				ImVec2 uv((b.x - a.x) / img.w, 0);
				win->DrawList->AddImage(img.texture, a, b, ImVec2(0, 0), uv);
				break;
			}
			break;
			case litehtml::background_repeat_repeat_y:
			{
				ImVec2 uv(0, (b.y - a.y) / img.h);
				win->DrawList->AddImage(img.texture, a, b, ImVec2(0, 0), uv);
				break;
			}
			case litehtml::background_repeat_repeat:
			{
				ImVec2 uv((b.x - a.x) / img.w, (b.y - a.y) / img.h);
				win->DrawList->AddImage(img.texture, a, b, ImVec2(0, 0), uv);
				break;
			}
		}
		/*cairo_t* cr = (cairo_t*)hdc;
		cairo_save(cr);
		apply_clip(cr);

		rounded_rectangle(cr, bg.border_box, bg.border_radius);
		cairo_clip(cr);

		cairo_rectangle(cr, bg.clip_box.x, bg.clip_box.y, bg.clip_box.width, bg.clip_box.height);
		cairo_clip(cr);

		if (bg.color.alpha)
		{
			set_color(cr, bg.color);
			cairo_paint(cr);
		}*/

	}


	void draw_borders(litehtml::uint_ptr hdc,
		const litehtml::borders& borders,
		const litehtml::position& draw_pos,
		bool root) override
	{
		ImGuiWindow* win = ImGui::GetCurrentWindow();
		//ImVec2 imgui_pos = { win->Pos.x + (float)draw_pos.x, win->Pos.y + (float)draw_pos.y };
		//win->DrawList->AddRect(imgui_pos, )

		auto drawBorder = [](const litehtml::border& border) {
		};

		if (borders.bottom.width != 0 && borders.bottom.style > litehtml::border_style_hidden)
		{
			ImVec2 a(win->Pos.x + draw_pos.left(), win->Pos.y + draw_pos.bottom());
			ImVec2 b(win->Pos.x + draw_pos.right(), win->Pos.y + draw_pos.bottom());
			ImColor col(borders.bottom.color.red, borders.bottom.color.green, borders.bottom.color.blue, borders.bottom.color.alpha);

			for (int x = 0; x < borders.bottom.width; x++)
			{
				win->DrawList->AddLine(a, b, col);
				++a.y;
				++b.y;
			}
		}

		if (borders.top.width != 0 && borders.top.style > litehtml::border_style_hidden)
		{
			ImVec2 a(win->Pos.x + draw_pos.left(), win->Pos.y + draw_pos.top());
			ImVec2 b(win->Pos.x + draw_pos.right(), win->Pos.y + draw_pos.top());
			ImColor col(borders.top.color.red, borders.top.color.green, borders.top.color.blue, borders.top.color.alpha);

			for (int x = 0; x < borders.top.width; x++)
			{
				win->DrawList->AddLine(a, b, col);
				++a.y;
				++b.y;
			}
		}

		if (borders.right.width != 0 && borders.right.style > litehtml::border_style_hidden)
		{
			ImVec2 a(win->Pos.x + draw_pos.right(), win->Pos.y + draw_pos.top());
			ImVec2 b(win->Pos.x + draw_pos.right(), win->Pos.y + draw_pos.bottom());
			ImColor col(borders.right.color.red, borders.right.color.green, borders.right.color.blue, borders.right.color.alpha);

			for (int x = 0; x < borders.right.width; x++)
			{
				win->DrawList->AddLine(a, b, col);
				--a.x;
				--b.x;
			}
		}

		if (borders.left.width != 0 && borders.left.style > litehtml::border_style_hidden)
		{
			ImVec2 a(win->Pos.x + draw_pos.left(), win->Pos.y + draw_pos.top());
			ImVec2 b(win->Pos.x + draw_pos.left(), win->Pos.y + draw_pos.bottom());
			ImColor col(borders.left.color.red, borders.left.color.green, borders.left.color.blue, borders.left.color.alpha);

			for (int x = 0; x < borders.left.width; x++)
			{
				win->DrawList->AddLine(a, b, col);
				++a.x;
				++b.x;
			}
		}
	}


	void set_caption(const litehtml::tchar_t* caption) override {}
	void set_base_url(const litehtml::tchar_t* base_url) override {
	
	}
	void link(const std::shared_ptr<litehtml::document>& doc, const litehtml::element::ptr& el) override {}
	void on_anchor_click(const litehtml::tchar_t* url, const litehtml::element::ptr& el) override {}
	void set_cursor(const litehtml::tchar_t* cursor) override {}
	void transform_text(litehtml::tstring& text, litehtml::text_transform tt) override {}
	void import_css(litehtml::tstring& text, const litehtml::tstring& url, litehtml::tstring& baseurl) override
	{
		Array<u8> data(m_app.getWorldEditor()->getAllocator());
		download(m_host, url.c_str(), &data);
		text = (const char*)&data[0];
	}
	void set_clip(const litehtml::position& pos,
		const litehtml::border_radiuses& bdr_radius,
		bool valid_x,
		bool valid_y) override
	{
	}
	void del_clip() override {}
	void get_client_rect(litehtml::position& client) const override {
		client.height = 1024;
		client.width = 1024;
		ImGuiWindow* win = ImGui::GetCurrentWindow();
		client.x = win->Pos.x;
		client.y = win->Pos.y;
	}
	std::shared_ptr<litehtml::element> create_element(const litehtml::tchar_t* tag_name,
		const litehtml::string_map& attributes,
		const std::shared_ptr<litehtml::document>& doc) override
	{
		return nullptr;
	}


	void get_media_features(litehtml::media_features& media) const override {
		/* TODO */
		litehtml::position client;
		get_client_rect(client);

		media.type = litehtml::media_type_screen;
		media.width = client.width;
		media.height = client.height;
		media.color = 8;
		media.monochrome = 0;
		media.color_index = 256;
		media.resolution = 96;
		media.device_width = 1024;
		media.device_height = 1024;
	}


	void get_language(litehtml::tstring& language, litehtml::tstring& culture) const override {}

	struct Image
	{
		ImTextureID texture;
		int w;
		int h;
	};

	HashMap<u32, Image> m_images;
	StudioApp& m_app;
	char m_host[256];
};


struct HTMLPlugin LUMIX_FINAL : public StudioApp::IPlugin
{
	HTMLPlugin(StudioApp& app)
		: m_app(app)
		, m_html_container(app)
	{
	}


	bool loadFile(const char* path, Array<u8>* data)
	{
		ASSERT(data);
		FS::OsFile file;
		IAllocator& allocator = m_app.getWorldEditor()->getAllocator();
		if (!file.open(path, FS::Mode::OPEN_AND_READ, allocator)) return false;

		data->resize(file.size() + 1);
		bool res = file.read(&(*data)[0], data->size());
		(*data)[data->size() - 1] = '\0';
		file.close();
		return res;
	}


	void onWindowGUI() override 
	{
		static bool first = true;
		static char host[256] = "google.com";
		static char path[256] = "http://www.google.com/ncr";
		if (first)
		{
			IAllocator& allocator = m_app.getWorldEditor()->getAllocator();
			Array<u8> css(allocator);
			loadFile("master.css", &css);

			strcpy_s(m_html_container.m_host, host);
			Array<u8> page(allocator);
			HTMLDocumentContainer::download(host, path, &page);
			//loadFile("index.html", &page);
			m_html_context.load_master_stylesheet((litehtml::tchar_t*)&css[0]);
			m_document = litehtml::document::createFromUTF8((const char*)&page[0], &m_html_container, &m_html_context);
			m_document->render(1024); // TODO

			first = false;
		}

		if (ImGui::BeginDock("HTML"))
		{
			if (ImGui::Button("refresh")) first = true;
			ImGui::SameLine();
			ImGui::InputText("Host", host, sizeof(host));
			ImGui::SameLine();
			ImGui::InputText("Path", path, sizeof(path));
			if (ImGui::BeginChildFrame(0123, ImVec2(0, 0)))
			{
				litehtml::position clip(0, 0, 1024, 1024);
				m_document->draw((litehtml::uint_ptr)this, 0, 0, &clip);
			}
			ImGui::EndChildFrame();
		}
		ImGui::EndDock();
	}


	const char* getName() const override { return "html"; }

	StudioApp& m_app;
	litehtml::context m_html_context;
	HTMLDocumentContainer m_html_container;
	std::shared_ptr<litehtml::document> m_document;
};


LUMIX_STUDIO_ENTRY(lumixengine_html)
{
	WorldEditor& editor = *app.getWorldEditor();

	auto* plugin = LUMIX_NEW(editor.getAllocator(), HTMLPlugin)(app);
	app.addPlugin(*plugin);
}
