#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/sysfs.h>

struct gpio_som_id {
	struct platform_device *pdev;
	struct pinctrl *pinctrl;
	struct gpio_desc *gpio[4];
	bool use_fixed_som_id;
	int fixed_som_id;
};

static ssize_t som_id_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct gpio_som_id *som_id = dev_get_drvdata(dev);
	int i, id = 0;

	if (!som_id)
		return -EINVAL;

	if (som_id->use_fixed_som_id)
		return sprintf(buf, "%d\n", som_id->fixed_som_id);

	for (i = 0; i < ARRAY_SIZE(som_id->gpio); i++)
		id |= gpiod_get_value(som_id->gpio[i]) << i;

	return sprintf(buf, "%d\n", id);
}

static ssize_t som_id_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct gpio_som_id *som_id = dev_get_drvdata(dev);
	int id, rc;

	if (!som_id)
		return -EINVAL;

	if (!som_id->use_fixed_som_id)
		return -EOPNOTSUPP;

	rc = kstrtoint(buf, 0, &id);
	if (rc)
		return rc;

	if (id < 0 || id > 15)
		return -EINVAL;

	som_id->fixed_som_id = id;
	return count;
}

static DEVICE_ATTR(som_id, 0644, som_id_show, som_id_store);

static struct attribute *som_id_attrs[] = {
	&dev_attr_som_id.attr,
	NULL
};

static struct attribute_group som_id_attr_group = {
	.attrs = som_id_attrs,
};

static int gpio_som_id_probe(struct platform_device *pdev)
{
	struct gpio_som_id *som_id;
	u32 fixed_som_id;
	int i, rc = 0;

	som_id = devm_kzalloc(&pdev->dev, sizeof(*som_id), GFP_KERNEL);
	if (!som_id)
		return -ENOMEM;

	som_id->pdev = pdev;
	platform_set_drvdata(pdev, som_id);

	if (!of_property_read_u32(pdev->dev.of_node, "comma,fixed-som-id",
				  &fixed_som_id)) {
		if (fixed_som_id > 15)
			return -EINVAL;

		som_id->use_fixed_som_id = true;
		som_id->fixed_som_id = fixed_som_id;
		goto create_sysfs;
	}

	som_id->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(som_id->pinctrl)) {
		dev_err(&pdev->dev, "Failed to get pinctrl: %ld\n",
			PTR_ERR(som_id->pinctrl));
		return PTR_ERR(som_id->pinctrl);
	}

	for (i = 0; i < ARRAY_SIZE(som_id->gpio); i++) {
		som_id->gpio[i] = devm_gpiod_get_index(&pdev->dev, NULL, i,
						       GPIOD_IN);
		if (IS_ERR(som_id->gpio[i])) {
			dev_err(&pdev->dev, "Failed to get GPIO: %ld\n",
				PTR_ERR(som_id->gpio[i]));
			return PTR_ERR(som_id->gpio[i]);
		}
	}

create_sysfs:
	rc = sysfs_create_group(&pdev->dev.kobj, &som_id_attr_group);
	if (rc)
		dev_err(&pdev->dev, "Failed to create sysfs entry: %d\n", rc);

	return rc;
}

static int gpio_som_id_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &som_id_attr_group);
	return 0;
}

static const struct of_device_id of_match_table[] = {
	{ .compatible = "comma,gpio-som-id", },
	{}
};

static struct platform_driver gpio_som_id_driver = {
	.driver = {
		.name = "comma,gpio-som-id",
		.of_match_table = of_match_table,
	},
	.probe = gpio_som_id_probe,
	.remove = gpio_som_id_remove,
};

module_driver(gpio_som_id_driver, platform_driver_register,
	      platform_driver_unregister);

MODULE_DESCRIPTION("GPIO SOM ID driver");
MODULE_LICENSE("MIT");
