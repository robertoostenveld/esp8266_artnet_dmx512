import javax.swing.*;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;
import java.awt.*;
import java.awt.font.FontRenderContext;
import java.awt.font.LineMetrics;
import java.awt.geom.Line2D;
import java.util.Hashtable;

import static java.awt.Color.*;
import static java.awt.RenderingHints.KEY_ANTIALIASING;
import static java.awt.RenderingHints.VALUE_ANTIALIAS_ON;
import static java.lang.String.format;
import static javax.swing.WindowConstants.EXIT_ON_CLOSE;

/**
 * @author: Artur Pogoda de la Vega
 * @date: 2022-11-12
 *
 * This is Java Swing application that will visualize a DMX signal created by DmxArray
 * merely for demonstration and debug purposes.
 */
public class VisualizeDMX implements ChangeListener {

    private final JScrollPane scroll;
    private final Graph graph;
    private final JFrame frame;
    private final JPanel top;
    private final LabeledSlider channels;
    private final LabeledSlider sfb;
    private final LabeledSlider mab;
    private final LabeledSlider mbb;
    private final LabeledSlider stops;
    private final DmxArray array;
    private final LabeledSlider ch1;
    private final LabeledSlider ch2;
    private final LabeledSlider ch3;
    private final LabeledSlider ch4;
    private final LabeledSlider ch5;
    private final LabeledSlider ch6;
    private final LabeledSlider ch7;
    private final LabeledSlider ch8;
    private final LabeledSlider chN;
    private final LabeledSlider zoom;
    private final JCheckBox ideal;

    public static void main(String[] args) {
        VisualizeDMX v = new VisualizeDMX();
        v.show();
    }

    public VisualizeDMX() {
        frame = new JFrame("DMX Visualizer");
        frame.setDefaultCloseOperation(EXIT_ON_CLOSE);
        Container content = frame.getContentPane();
        content.setLayout(new BorderLayout());

        top = new JPanel();

        top.add(channels = new LabeledSlider("Channels", 4, 512, 192, 128, 192, 256, 512));
        top.add(sfb = new LabeledSlider("SFB", 1, 160, 28, 1, 40, 80, 120, 160));
        top.add(mab = new LabeledSlider("MAB", 1, 32, 3, 1, 8, 16, 24, 32));
        top.add(mbb = new LabeledSlider("MBB",1, 1000, 213, 1, 250, 500, 750, 1000));
        top.add(stops = new LabeledSlider("Stop bits",1, 17, 7, 1, 10, 20));
        top.add(new JLabel("         "));
        top.add(ch1 = new LabeledSlider("CH1",0, 255, 0, 0, 128, 255));
        top.add(ch2 = new LabeledSlider("CH2",0, 255, 0, 0, 128, 255));
        top.add(ch3 = new LabeledSlider("CH3",0, 255, 0, 0, 128, 255));
        top.add(ch4 = new LabeledSlider("CH4",0, 255, 0, 0, 128, 255));
        top.add(ch5 = new LabeledSlider("CH5",0, 255, 0, 0, 128, 255));
        top.add(ch6 = new LabeledSlider("CH6",0, 255, 0, 0, 128, 255));
        top.add(ch7 = new LabeledSlider("CH7",0, 255, 0, 0, 128, 255));
        top.add(ch8 = new LabeledSlider("CH8",0, 255, 0, 0, 128, 255));
        top.add(chN = new LabeledSlider("CHN",0, 255, 0, 0, 128, 255));

        top.add(new JLabel("         "));
        top.add(zoom = new LabeledSlider("Zoom",1, 8, 1, 1, 2, 3, 4, 5, 6, 7, 8));
        top.add(new JLabel("         "));
        top.add(ideal = new JCheckBox("ideal"));

        channels.slider.addChangeListener(this);
        sfb.slider.addChangeListener(this);
        mab.slider.addChangeListener(this);
        mbb.slider.addChangeListener(this);
        stops.slider.addChangeListener(this);
        ch1.slider.addChangeListener(this);
        ch2.slider.addChangeListener(this);
        ch3.slider.addChangeListener(this);
        ch4.slider.addChangeListener(this);
        ch5.slider.addChangeListener(this);
        ch6.slider.addChangeListener(this);
        ch7.slider.addChangeListener(this);
        ch8.slider.addChangeListener(this);
        chN.slider.addChangeListener(this);
        zoom.slider.addChangeListener(this);
        ideal.addChangeListener(this);

        array = new DmxArray(channels.getValue(), mbb.getValue(), sfb.slider.getValue(), mab.getValue(), stops.getValue());
        graph = new Graph(array);
        scroll = new JScrollPane(graph);

        content.add(top, BorderLayout.NORTH);
        content.add(scroll, BorderLayout.CENTER);

        Dimension size = Toolkit.getDefaultToolkit().getScreenSize();

        frame.pack();
        frame.setSize((int)size.getWidth(), 500);
    }

    @Override
    public void stateChanged(ChangeEvent changeEvent) {
        array.reconfig(channels.getValue(), mbb.getValue(), sfb.getValue(), mab.getValue(), stops.getValue());
        array.setChannel(1, ch1.getValue());
        array.setChannel(2, ch2.getValue());
        array.setChannel(3, ch3.getValue());
        array.setChannel(4, ch4.getValue());
        array.setChannel(5, ch5.getValue());
        array.setChannel(6, ch6.getValue());
        array.setChannel(7, ch7.getValue());
        array.setChannel(8, ch8.getValue());
        array.setChannel(array.getNumChannels(), chN.getValue());
        graph.setScale(zoom.getValue());
        graph.setIdeal(ideal.isSelected());
        graph.repaint();
    }

    private void show() {
        frame.setVisible(true);
    }
}

class Graph extends JPanel {

    private boolean ideal;

    Graph(BitArray data) {
        this.data = data;
        setBackground(BLACK);
    }

    void setScale(int scale) {
        this.scale = scale;
    }
    int scale = 1;

    @Override
    public Dimension getPreferredSize() {
        return new Dimension(12000, 100);
    }

    BitArray data;
    final int PAD = 20;

    protected void paintComponent(Graphics g) {
        super.paintComponent(g);
        Graphics2D g2 = (Graphics2D)g;
        g2.setColor(GRAY);
        g2.setRenderingHint(KEY_ANTIALIASING, VALUE_ANTIALIAS_ON);
        int w = getWidth();
        int h = getHeight();
        g2.draw(new Line2D.Double(PAD, PAD, PAD, h-PAD));
        g2.draw(new Line2D.Double(PAD, h-PAD, w-PAD, h-PAD));
        Font font = g2.getFont();
        FontRenderContext frc = g2.getFontRenderContext();
        LineMetrics lm = font.getLineMetrics("0", frc);
        float sh = lm.getAscent() + lm.getDescent();

        int numBytes = data.size();
        int numBits = data.getNumBits();
        int padding = data.getPadding();
        int micros = 4 * numBits;
        double fps = 1000000.0/micros;

        int y0 = h-PAD;
        int y2 = PAD+PAD;

        int max = getWidth()-100/8;
        for (int i=0; i<=max; i++) {
            g2.setColor(0==i%4 ? GRAY : DARK_GRAY);
            int x = (8*i*scale)+PAD;
            g2.draw(new Line2D.Double(x, y0, x, PAD+4));
        }

        String s = format("bytes: %d, bits: %d, padding: %d bits, time: %d μs, max. rate: %3.1f packets/s",
                numBytes, numBits, padding, micros, fps);

        g2.drawString(s, PAD+4, PAD);

        g.setColor(YELLOW);
        int unpadded = numBits-padding;
        boolean last = false;
        for (int bit=0; bit<numBits; bit++) {
            if (bit>=unpadded) {
                g.setColor(ORANGE.darker());
            } else {
                g.setColor(YELLOW);
            }
            boolean set = data.isSet(bit);
            int x1 = PAD+(bit*scale);
            int x2 = PAD+(bit*scale)+scale-1;
            for (int x=x1; x<x2; x++) {
                //g2.draw(new Line2D.Double(x, y0, x, set ? y2 : y1));
            }

            int slew = ideal ? 0 : scale/3;
            int jitter = ideal ? 0 : scale<2 ? 0 : 1;
            int over = ideal ? 0 : 1;
            if (last && set) {
                g2.draw(new Line2D.Double(x1, y2, x2, y2+jitter)); // high
            } else if (!last && !set) {
                g2.draw(new Line2D.Double(x1, y0, x2, y0-jitter)); // low
            } else if (!last && set) {
                g2.draw(new Line2D.Double(x1, y0-1, x1+slew, y2-over)); // raising edge
                g2.draw(new Line2D.Double(x1+slew, y2-jitter, x2, y2)); // high
            } else if (last && !set) {
                g2.draw(new Line2D.Double(x1, y2+2*over, x1+slew, y0+over)); // falling edge
                g2.draw(new Line2D.Double(x1+slew, y0+jitter, x2, y0)); // high
            }

            last = set;
        }
    }

    public void setIdeal(boolean ideal) {
        this.ideal = ideal;
    }
}

class LabeledSlider extends JPanel implements ChangeListener {
    final JSlider slider;
    private final JLabel current;

    public LabeledSlider(String name, int min, int max, int value, int ... ticks) {
        super(new BorderLayout());
        JLabel label = new JLabel(name);
        slider = new JSlider(JSlider.VERTICAL, min, max, value);
        current = new JLabel("" + value);
        slider.addChangeListener(this);
        Hashtable<Integer,JLabel> position = new Hashtable<>();
        if (ticks.length<1) {
            position.put(1, new JLabel("1"));
            position.put(128, new JLabel("128"));
            position.put(256, new JLabel("256"));
            position.put(384, new JLabel("384"));
            position.put(512, new JLabel("512"));
        } else {
            for (int t : ticks) {
                position.put(t, new JLabel(format("%3d", t)));
            }
        }
        slider.setMinorTickSpacing(16);
        slider.setPaintTicks(true);
        slider.setPaintLabels(true);
        slider.setLabelTable(position);
        add(label, BorderLayout.NORTH);
        add(slider, BorderLayout.CENTER);
        add(current, BorderLayout.SOUTH);
    }

    @Override
    public void stateChanged(ChangeEvent changeEvent) {
        current.setText(format("%3d", slider.getValue()));
    }

    public int getValue() {
        return slider.getValue();
    }
}

